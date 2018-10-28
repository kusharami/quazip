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

#include "quaziodeviceprivate.h"

#include "quaziodevice.h"

QuaZIODevicePrivate::QuaZIODevicePrivate(QuaZIODevice *owner)
    : owner(owner)
    , io(nullptr)
    , ioStartPosition(0)
    , ioPosition(0)
    , compressionLevel(Z_DEFAULT_COMPRESSION)
    , strategy(Z_DEFAULT_STRATEGY)
    , uncompressedSize(0)
    , hasError(false)
    , atEnd(false)
    , hasUncompressedSize(false)
    , transaction(false)
{
    memset(&zstream, 0, sizeof(zstream));
}

QuaZIODevicePrivate::~QuaZIODevicePrivate()
{
    // do nothing
}

bool QuaZIODevicePrivate::flushBuffer(int size)
{
    if (size == 0)
        return true;

    if (io->write(reinterpret_cast<char *>(zbuffer), size) == size) {
        ioPosition += size;
        zstream.next_out = zbuffer;
        zstream.avail_out = sizeof(zbuffer);
        return true;
    }

    setError(io->errorString());
    return false;
}

bool QuaZIODevicePrivate::seekInternal(qint64 newPos)
{
    if (!owner->isReadable())
        return false;

    if (io->isSequential())
        return true;

    auto maxSize = maxUncompressedSize();

    if (newPos > qint64(maxSize))
        return false;

    hasError = false;
    owner->setErrorString(QString());

    qint64 skipCount = qint64(zstream.total_out);
    skipCount -= newPos;
    if (skipCount > 0 || zstream.total_out > maxSize) {
        if (!doInflateReset()) {
            return false;
        }

        atEnd = false;
        ioPosition = ioStartPosition;

        zstream.next_in = zbuffer;
        zstream.avail_in = 0;
        skipCount = newPos;
    } else {
        skipCount = -skipCount;
    }

    return skip(skipCount);
}

bool QuaZIODevicePrivate::skip(qint64 skipCount)
{
    int blockSize = QUAZIO_BUFFER_SIZE;
    if (hasUncompressedSize) {
        QuaZUtils::adjustBlockSize(blockSize, qint64(uncompressedSize));
    }

    while (skipCount > 0) {
        QuaZUtils::adjustBlockSize(blockSize, skipCount);
        if (seekBuffer.size() < blockSize) {
            seekBuffer.resize(blockSize);
        }

        auto readBytes = readInternal(seekBuffer.data(), blockSize);
        if (readBytes != blockSize) {
            return false;
        }
        skipCount -= readBytes;
    }

    return true;
}

qint64 QuaZIODevicePrivate::readCompressedData(Bytef *zbuffer, size_t size)
{
    auto readResult = io->read(reinterpret_cast<char *>(zbuffer), qint64(size));

    if (readResult <= 0) {
        setError(readResult < 0 ? io->errorString()
                                : QStringLiteral("Unexpected end of file."));
    }

    return readResult;
}

void QuaZIODevicePrivate::finishReadTransaction(qint64 savedPosition)
{
    if (!transaction)
        return;

    Q_ASSERT(io);
    Q_ASSERT(io->isTransactionStarted());
    io->rollbackTransaction();
    auto skipCount = ioPosition - savedPosition;
    while (skipCount > 0) {
        Byte buf[4096];
        auto skipSize = readCompressedData(
            buf, size_t(std::min(skipCount, qint64(sizeof(buf)))));
        if (skipSize <= 0) {
            return;
        }

        skipCount -= skipSize;
    }
    transaction = false;
}

void QuaZIODevicePrivate::setCompressionLevel(int level)
{
    if (level == compressionLevel)
        return;

    compressionLevel = level;

    if (owner->isWritable()) {
        check(deflateParams(&zstream, compressionLevel, strategy));
    }
}

void QuaZIODevicePrivate::setStrategy(int value)
{
    if (value == strategy)
        return;

    strategy = value;

    if (owner->isWritable()) {
        check(deflateParams(&zstream, compressionLevel, strategy));
    }
}

qint64 QuaZIODevicePrivate::readInternal(char *data, qint64 maxlen)
{
    if (hasError || !io->isReadable() || !seekInit()) {
        return -1;
    }

    if (io->isTextModeEnabled()) {
        setError("Source device is not binary.");
        return -1;
    }

    if (maxlen <= 0)
        return maxlen;

    if (atEnd) {
        return 0;
    }

    using DataType = decltype(zstream.next_out);
    using BlockSize = decltype(zstream.avail_out);

    zstream.next_out = reinterpret_cast<DataType>(data);

    qint64 count = maxlen;
    auto blockSize = QuaZUtils::maxBlockSize<BlockSize>();
    bool run = true;

    auto savedPosition = ioPosition;

    while (!hasError && run && count > 0) {
        QuaZUtils::adjustBlockSize(blockSize, count);

        zstream.avail_out = blockSize;

        while (!hasError && run && zstream.avail_out > 0) {
            if (zstream.avail_in == 0) {
                if (transaction)
                    io->commitTransaction();

                transaction = !io->isTransactionStarted() && io->isSequential();
                if (transaction) {
                    io->startTransaction();
                }
                auto readResult = readCompressedData(zbuffer, sizeof(zbuffer));
                if (readResult <= 0) {
                    run = false;
                    break;
                }

                zstream.avail_in = BlockSize(readResult);
                zstream.next_in = zbuffer;
                ioPosition += zstream.avail_in;
            }

            int code = inflate(&zstream, Z_NO_FLUSH);
            if (code == Z_STREAM_END) {
                run = false;
                atEnd = true;
                hasUncompressedSize = true;
                uncompressedSize = zstream.total_out;
                ioPosition -= zstream.avail_in;
                finishReadTransaction(savedPosition);
                break;
            }

            if (code == Z_NEED_DICT) {
                setError("External zlib dictionary is not supported.");
                break;
            }

            if (!check(code)) {
                break;
            }
        }

        count -= qint64(blockSize - zstream.avail_out);
    }

    if (hasError) {
        return -1;
    }

    return maxlen - count;
}

bool QuaZIODevicePrivate::initRead()
{
    if (!io->isReadable()) {
        setError("Source device is not readable.");
        return false;
    }

    atEnd = false;

    zstream.next_in = zbuffer;
    zstream.avail_in = 0;

    return doInflateInit();
}

qint64 QuaZIODevicePrivate::writeInternal(const char *data, qint64 maxlen)
{
    if (hasError || !io->isWritable() || !seekInit()) {
        return -1;
    }

    if (io->isTextModeEnabled()) {
        setError("Target device is not binary.");
        return -1;
    }

    if (maxlen <= 0)
        return maxlen;

    using DataType = decltype(zstream.next_in);
    using BlockSize = decltype(zstream.avail_in);

    qint64 count = maxlen;
    auto blockSize = QuaZUtils::maxBlockSize<BlockSize>();

    zstream.next_in = reinterpret_cast<DataType>(data);

    while (count > 0) {
        QuaZUtils::adjustBlockSize(blockSize, count);

        zstream.avail_in = blockSize;

        while (zstream.avail_in > 0) {
            if (!check(deflate(&zstream, Z_NO_FLUSH))) {
                return -1;
            }
            if (zstream.avail_out == 0) {
                if (!flushBuffer()) {
                    return -1;
                }
            }
        }

        count -= qint64(blockSize);
    }

    return maxlen - count;
}

bool QuaZIODevicePrivate::seekInit()
{
    if (!io->isSequential() && !io->seek(ioPosition)) {
        setError("Dependent device seek failed.");
        return false;
    }

    return true;
}

bool QuaZIODevicePrivate::check(int code)
{
    if (code < Z_OK) {
        setError(QLatin1String(zstream.msg));
        return false;
    }

    return true;
}

bool QuaZIODevicePrivate::initWrite()
{
    if (!io->isWritable()) {
        setError("Target device is not writable.");
        return false;
    }

    zstream.next_out = zbuffer;
    zstream.avail_out = sizeof(zbuffer);

    return doDeflateInit();
}

bool QuaZIODevicePrivate::doInflateInit()
{
    return check(inflateInit(&zstream));
}

bool QuaZIODevicePrivate::doInflateReset()
{
    return check(inflateReset(&zstream));
}

bool QuaZIODevicePrivate::doDeflateInit()
{
    return check(deflateInit2(&zstream, compressionLevel, Z_DEFLATED, MAX_WBITS,
        MAX_MEM_LEVEL, strategy));
}

void QuaZIODevicePrivate::endRead()
{
    Q_ASSERT(owner->isReadable());
    if (transaction) {
        if (hasError)
            io->rollbackTransaction();
        else
            io->commitTransaction();
        transaction = false;
    }
    seekInit();
    check(inflateEnd(&zstream));
}

void QuaZIODevicePrivate::endWrite()
{
    Q_ASSERT(owner->isWritable());
    zstream.next_in = Z_NULL;
    zstream.avail_in = 0;
    if (seekInit()) {
        while (!hasError) {
            int result = deflate(&zstream, Z_FINISH);
            if (result != Z_BUF_ERROR && !check(result))
                break;

            if (result == Z_STREAM_END)
                break;

            Q_ASSERT(zstream.avail_out == 0);
            if (!flushBuffer())
                break;
        }
    }

    if (!hasError && zstream.avail_out < sizeof(zbuffer)) {
        flushBuffer(int(sizeof(zbuffer) - zstream.avail_out));
    }
    if (!hasError) {
        seekInit(); // HACK: ensure QFileDevice flushed
    }
    check(deflateEnd(&zstream));
}

void QuaZIODevicePrivate::setError(const QString &message)
{
    hasError = true;
    owner->setErrorString(message);
}
