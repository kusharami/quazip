#ifndef QUAZIP_QUAZIODEVICE_H
#define QUAZIP_QUAZIODEVICE_H

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

#include "quazip_global.h"
#include <QIODevice>

class QuaZIODevicePrivate;

/// A class to compress/decompress QIODevice.
/**
  This class can be used to compress any data written to QIODevice or
  decompress it back. Compressing data sent over a QTcpSocket is a good
  example.
  */
class QUAZIP_EXPORT QuaZIODevice : public QIODevice {
    Q_OBJECT
    friend class QuaZIODevicePrivate;

public:
    /// Constructor.
    explicit QuaZIODevice(QObject *parent = nullptr);
    /// Constructor.
    /**
      \param io The QIODevice to read/write.
      \param parent The parent object, as per QObject logic.
      */
    QuaZIODevice(QIODevice *io, QObject *parent = nullptr);
    /// Destructor.
    virtual ~QuaZIODevice() override;
    /// Opens the device.
    /**
      \param mode Neither QIODevice::ReadWrite nor QIODevice::Append are
      not supported.
      */
    virtual bool open(QIODevice::OpenMode mode) override;
    /// Closes this device, but not the underlying one.
    /**
      The underlying QIODevice is not closed in case you want to write
      something else to it.
      */
    virtual void close() override;
    /// Sets dependent IO device. This device is closed.
    void setIODevice(QIODevice *device);
    /// Returns the underlying device.
    QIODevice *getIODevice() const;
    /// Returns true if the end of the compressed stream is reached.
    virtual bool atEnd() const override;
    /// Returns if device is sequential.
    virtual bool isSequential() const override;
    /// Returns the number of the bytes buffered.
    virtual qint64 bytesAvailable() const override;

    /// Returns the size of bytes written for write-only mode.
    /// Returns the size of uncompressed data for read-only mode. May be slow.
    virtual qint64 size() const override;

    /// Indicates if there was an error on last executed operation.
    bool hasError() const;

    /// Sets th compression level (Z_BEST_COMPRESSION etc.).
    /// Device is closed.
    void setCompressionLevel(int level);
    /// Compression level
    int compressionLevel() const;

protected:
    /// protected constructor for descendants
    QuaZIODevice(QuaZIODevicePrivate *p, QObject *parent);

    /// Implementation of QIODevice::readData().
    virtual qint64 readData(char *data, qint64 maxSize) override;
    /// Implementation of QIODevice::writeData().
    virtual qint64 writeData(const char *data, qint64 maxSize) override;

private:
    void dependedDeviceWillClose();
    void dependentDeviceDestoyed();

protected:
    QuaZIODevicePrivate *d;
};

#endif // QUAZIP_QUAZIODEVICE_H
