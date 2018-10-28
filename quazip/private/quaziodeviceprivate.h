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

#pragma once

#include "quazutils.h"

#include <QByteArray>
#include <zlib.h>

class QIODevice;
class QuaZIODevice;

enum
{
    QUAZIO_BUFFER_SIZE = 32768
};

/// \cond internal
class QuaZIODevicePrivate {
public:
    using SizeType = decltype(z_stream::total_out);

    QuaZIODevice *owner;
    QIODevice *io;
    qint64 ioStartPosition;
    qint64 ioPosition;
    int compressionLevel;
    int strategy;
    SizeType uncompressedSize;
    bool hasError : 1;
    bool atEnd : 1;
    bool hasUncompressedSize : 1;
    bool transaction : 1;
    QByteArray seekBuffer;
    z_stream zstream;
    Byte zbuffer[QUAZIO_BUFFER_SIZE];

    QuaZIODevicePrivate(QuaZIODevice *owner);
    virtual ~QuaZIODevicePrivate();

    bool initRead();
    bool initWrite();

    virtual bool doInflateInit();
    virtual bool doInflateReset();
    virtual bool doDeflateInit();

    bool flushBuffer(int size = QUAZIO_BUFFER_SIZE);
    bool seekInternal(qint64 newPos);
    bool skip(qint64 skipCount);
    qint64 readInternal(char *data, qint64 maxlen);
    qint64 writeInternal(const char *data, qint64 maxlen);
    bool seekInit();
    bool check(int code);
    void endRead();
    void endWrite();
    void setError(const QString &message);
    qint64 readCompressedData(Bytef *zbuffer, size_t size);
    void finishReadTransaction(qint64 savedPosition);
    void setCompressionLevel(int level);
    void setStrategy(int value);

    static inline Q_DECL_CONSTEXPR SizeType maxUncompressedSize();
};

QuaZIODevicePrivate::SizeType Q_DECL_CONSTEXPR
QuaZIODevicePrivate::maxUncompressedSize()
{
    return QuaZUtils::maxBlockSize<SizeType>();
}
