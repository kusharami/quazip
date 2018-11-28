/*
Copyright (C) 2005-2014 Sergey A. Tachenov
Copyright (C) 2018 Alexandra Cherdantseva

This file is part of QuaZIP.

QuaZIP is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 of the License, or
(at your option) any later version.

QuaZIP is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with QuaZIP.  If not, see <http://www.gnu.org/licenses/>.

See COPYING file for the full LGPL text.

Original ZIP package is copyrighted by Gilles Vollant and contributors,
see quazip/(un)zip.h files for details. Basically it's the zlib license.
*/

#include "quaziodevice.h"

#include "private/quaziodeviceprivate.h"

QuaZIODevice::QuaZIODevice(QuaZIODevicePrivate *p, QObject *parent)
    : QIODevice(parent)
    , d(p)
{
    Q_ASSERT(p);
}

QuaZIODevice::QuaZIODevice(QObject *parent)
    : QuaZIODevice(new QuaZIODevicePrivate(this), parent)
{
}

QuaZIODevice::QuaZIODevice(QIODevice *io, QObject *parent)
    : QuaZIODevice(parent)
{
    setIODevice(io);
}

QuaZIODevice::~QuaZIODevice()
{
    setIODevice(nullptr);
    delete d;
}

QIODevice *QuaZIODevice::ioDevice() const
{
    return d->io;
}

bool QuaZIODevice::atEnd() const
{
    return bytesAvailable() == 0;
}

bool QuaZIODevice::open(OpenMode mode)
{
    if (d->io == nullptr) {
        d->setError("Dependent device is not set.");
        return false;
    }

    if (mode & (WriteOnly | Truncate)) {
        mode |= WriteOnly | Truncate;
    }

    mode |= Unbuffered;

    if (isOpen()) {
        qWarning("QuaZIODevice is already open");
        Q_ASSERT(mode == openMode());
        return false;
    }

    if (mode & Append) {
        d->setError("Append is not supported for zlib compressed device.");
        return false;
    }

    if ((mode & ReadWrite) == ReadWrite) {
        d->setError(
            "Zlib device should be opened in read-only or write-only mode.");
        return false;
    }

    if (d->io->isTextModeEnabled()) {
        d->setError("Dependent device is not binary.");
        return false;
    }

    if (!d->io->isOpen()) {
        if (!d->io->open(mode & ~(Text | Unbuffered))) {
            d->setError("Dependent device could not be opened.");
            return false;
        }
    }

    d->hasError = false;
    setErrorString(QString());
    setOpenMode(mode);
    d->hasUncompressedSize = false;

    if (mode & ReadOnly) {
        if (!d->initRead()) {
            setOpenMode(NotOpen);
            return false;
        }
    }

    if (mode & WriteOnly) {
        if (!d->initWrite()) {
            setOpenMode(NotOpen);
            return false;
        }
    }

    Q_ASSERT(!d->hasError);
    d->ioPosition = d->ioStartPosition;
    return QIODevice::open(mode);
}

void QuaZIODevice::close()
{
    if (!isOpen())
        return;

    QString errorString;
    if (d->hasError)
        errorString = this->errorString();
    auto savedOpenMode = openMode();
    QIODevice::close();
    setOpenMode(savedOpenMode);
    if (isReadable()) {
        d->endRead();
    } else if (isWritable()) {
        d->endWrite();
    } else {
        Q_UNREACHABLE();
    }
    setOpenMode(NotOpen);

    if (!d->hasError && !errorString.isEmpty()) {
        d->setError(errorString);
    }
}

void QuaZIODevice::setIODevice(QIODevice *device)
{
    auto io = d->io;
    if (io == device)
        return;

    close();

    if (io) {
        disconnect(io, &QIODevice::readyRead, this, &QuaZIODevice::readyRead);
        disconnect(io, &QIODevice::aboutToClose, this,
            &QuaZIODevice::dependedDeviceWillClose);
        disconnect(io, &QObject::destroyed, this,
            &QuaZIODevice::dependentDeviceDestoyed);
    }

    d->io = device;

    if (device) {
        connect(device, &QIODevice::readyRead, this, &QuaZIODevice::readyRead);
        connect(device, &QIODevice::aboutToClose, this,
            &QuaZIODevice::dependedDeviceWillClose);
        connect(device, &QObject::destroyed, this,
            &QuaZIODevice::dependentDeviceDestoyed);
        d->ioStartPosition = device->pos();
    }
}

qint64 QuaZIODevice::readData(char *data, qint64 maxSize)
{
    if (isReadable() && d->seekInternal(pos())) {
        return d->readInternal(data, maxSize);
    }

    return -1;
}

qint64 QuaZIODevice::writeData(const char *data, qint64 maxSize)
{
    return d->writeInternal(data, maxSize);
}

void QuaZIODevice::dependedDeviceWillClose()
{
    if (!isOpen())
        return;

    close();
    if (d->io->isWritable() && !d->hasError && d->io->bytesToWrite() != 0) {
        d->setError("Unable to flush compressed data.");
    }
}

void QuaZIODevice::dependentDeviceDestoyed()
{
    Q_ASSERT(!isOpen());
    d->io = nullptr;
}

bool QuaZIODevice::isSequential() const
{
    if (isReadable())
        return d->io->isSequential();

    return true;
}

qint64 QuaZIODevice::bytesAvailable() const
{
    if (!isOpen())
        return 0;

    if (d->hasError)
        return 0;

    if (isReadable()) {
        return size() - pos();
    }

    return 0;
}

qint64 QuaZIODevice::size() const
{
    if (isWritable())
        return qint64(d->zstream.total_in);

    if (isReadable()) {
        if (!d->hasUncompressedSize) {
            auto io = d->io;
            bool sequential = io->isSequential();
            if (!sequential || !isTransactionStarted()) {
                if (sequential) {
                    d->owner->startTransaction();
                    d->skipInput(QuaZIODevicePrivate::maxUncompressedSize());
                    d->owner->rollbackTransaction();
                } else {
                    d->skip(QuaZIODevicePrivate::maxUncompressedSize());
                }

                if (hasError()) {
                    return qint64(d->zstream.total_out);
                }
            }
        }

        return qint64(d->uncompressedSize);
    }

    return 0;
}

bool QuaZIODevice::hasError() const
{
    return d->hasError;
}

void QuaZIODevice::setCompressionLevel(int level)
{
    d->setCompressionLevel(level);
}

int QuaZIODevice::compressionStrategy() const
{
    return d->strategy;
}

void QuaZIODevice::setCompressionStrategy(int value)
{
    d->setStrategy(value);
}

int QuaZIODevice::compressionLevel() const
{
    return d->compressionLevel;
}
