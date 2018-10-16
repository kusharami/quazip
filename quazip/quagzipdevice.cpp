/*
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

#include "quagzipdevice.h"

#include "private/quaziodeviceprivate.h"

#include <QDateTime>

#include <memory>

enum
{
    FILENAME_MAX_ = FILENAME_MAX,
    COMMENT_MAX = 4096,
    EXTRA_MAX = 4096
};

class QuaGzipDevicePrivate : public QuaZIODevicePrivate {
public:
    QuaGzipDevicePrivate(QuaGzipDevice *owner);

    virtual bool doInflateInit() override;
    virtual bool doInflateReset() override;
    virtual bool doDeflateInit() override;

    bool gzInflateInit();
    bool gzDeflateInit();
    bool gzReadHeader();
    bool gzSetHeader();
    void gzInitHeader();

    char storedFileName[FILENAME_MAX_ + 1];
    char comment[COMMENT_MAX + 1];
    char extraField[EXTRA_MAX];
    gz_header gzHeader;
};

QuaGzipDevice::QuaGzipDevice(QObject *parent)
    : QuaZIODevice(new QuaGzipDevicePrivate(this), parent)
{
}

QuaGzipDevice::QuaGzipDevice(QIODevice *io, QObject *parent)
    : QuaGzipDevice(parent)
{
    setIODevice(io);
}

QuaGzipDevice::~QuaGzipDevice()
{
    setIODevice(nullptr);
}

int QuaGzipDevice::maxFileNameLength()
{
    return FILENAME_MAX_;
}

int QuaGzipDevice::maxCommentLength()
{
    return COMMENT_MAX;
}

bool QuaGzipDevice::headerIsProcessed() const
{
    return d()->gzHeader.done != 0;
}

QByteArray QuaGzipDevice::storedFileName() const
{
    return QByteArray(d()->storedFileName);
}

void QuaGzipDevice::setStoredFileName(const QByteArray &fileName)
{
    int length = fileName.length();
    if (length > FILENAME_MAX_) {
        d()->setError(QStringLiteral(
            "Unable to set more than %1 bytes for stored file name.")
                          .arg(FILENAME_MAX_));
        return;
    }
    memcpy(d()->storedFileName, fileName.data(), length);
    d()->storedFileName[length] = 0;
}

QByteArray QuaGzipDevice::comment() const
{
    return QByteArray(d()->comment);
}

void QuaGzipDevice::setComment(const QByteArray &text)
{
    auto comment = text;
    comment.replace('\r', QByteArray());
    int length = comment.length();
    if (length > maxCommentLength()) {
        d()->setError(QStringLiteral(
            "Unable to set more than %1 bytes for stored comment.")
                          .arg(maxCommentLength()));
        return;
    }
    memcpy(d()->comment, comment.data(), length);
    d()->comment[length] = 0;
}

time_t QuaGzipDevice::modificationTime() const
{
    return d()->gzHeader.time;
}

void QuaGzipDevice::setModificationTime(time_t time)
{
    d()->gzHeader.time = uLong(time);
}

QuaGzipDevice::ExtraFieldMap QuaGzipDevice::extraFields() const
{
    ExtraFieldMap result;

    auto &header = d()->gzHeader;

    auto extra = header.extra;
    auto count = header.extra_len;

    while (count >= 4) {
        ExtraFieldKey key(extra[0], extra[1]);
        auto len = quint16(extra[2] | (extra[3] << 8));

        count -= 4;
        extra += 4;
        if (count < len)
            break;

        result[key] = QByteArray(reinterpret_cast<const char *>(extra), len);

        count -= len;
        extra += len;
    }

    return result;
}

void QuaGzipDevice::setExtraFields(const ExtraFieldMap &map)
{
    auto &header = d()->gzHeader;
    auto extra = header.extra;

    auto extraMax = header.extra_max;
    header.extra_len = 0;

    for (auto it = map.begin(); it != map.end(); ++it) {
        auto &key = it.key();
        auto &value = it.value();

        int length = value.length();
        int maxFieldLength = std::numeric_limits<quint16>::max();

        if (length > maxFieldLength) {
            d()->setError(QStringLiteral(
                "Unable to set more than %1 bytes for extra field data.")
                              .arg(maxFieldLength));
            return;
        }

        if (header.extra_len + 4 + length > extraMax) {
            d()->setError(QStringLiteral(
                "Unable to set more than %1 bytes for extra field.")
                              .arg(extraMax));
            return;
        }

        extra[0] = key.key[0];
        extra[1] = key.key[1];

        extra[2] = quint8(length);
        extra[3] = quint8(length >> 8);

        memcpy(&extra[4], value.data(), length);

        header.extra_len += 4 + length;
        extra += 4 + length;
    }
}

QuaGzipDevicePrivate *QuaGzipDevice::d() const
{
    return static_cast<QuaGzipDevicePrivate *>(QuaZIODevice::d);
}

QuaGzipDevicePrivate::QuaGzipDevicePrivate(QuaGzipDevice *owner)
    : QuaZIODevicePrivate(owner)
{
    memset(&gzHeader, 0, sizeof(gzHeader));
    storedFileName[0] = 0;
    comment[0] = 0;
    gzHeader.extra = reinterpret_cast<Bytef *>(extraField);
    gzHeader.extra_max = EXTRA_MAX;
    gzHeader.name = reinterpret_cast<Bytef *>(storedFileName);
    gzHeader.name_max = FILENAME_MAX_;
    gzHeader.comment = reinterpret_cast<Bytef *>(comment);
    gzHeader.comm_max = COMMENT_MAX;
}

bool QuaGzipDevicePrivate::doInflateInit()
{
    return gzInflateInit() && gzReadHeader();
}

bool QuaGzipDevicePrivate::doInflateReset()
{
    return QuaZIODevicePrivate::doInflateReset() && gzReadHeader();
}

bool QuaGzipDevicePrivate::doDeflateInit()
{
    return gzDeflateInit() && gzSetHeader();
}

bool QuaGzipDevicePrivate::gzInflateInit()
{
    return check(inflateInit2(&zstream, QuaGzipDevice::GZIP_FLAG));
}

bool QuaGzipDevicePrivate::gzDeflateInit()
{
    return check(deflateInit2(&zstream, compressionLevel, Z_DEFLATED,
        MAX_WBITS | QuaGzipDevice::GZIP_FLAG, MAX_MEM_LEVEL,
        Z_DEFAULT_STRATEGY));
}

bool QuaGzipDevicePrivate::gzReadHeader()
{
    gzInitHeader();
    storedFileName[FILENAME_MAX_] = 0;
    comment[COMMENT_MAX] = 0;
    return check(inflateGetHeader(&zstream, &gzHeader));
}

bool QuaGzipDevicePrivate::gzSetHeader()
{
    gzInitHeader();
    if (gzHeader.time == 0)
        gzHeader.time = uLong(QDateTime::currentSecsSinceEpoch());
    gzHeader.done = 1;
    return check(deflateSetHeader(&zstream, &gzHeader));
}

void QuaGzipDevicePrivate::gzInitHeader()
{
    gzHeader.os = 255;
    gzHeader.text = owner->isTextModeEnabled() ? 1 : 0;
    gzHeader.hcrc = 0;
    gzHeader.done = 0;
    gzHeader.xflags = 0;
}
