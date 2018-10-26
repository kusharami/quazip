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
#include <QFileDevice>
#include <QFileInfo>
#include <QTextCodec>
#include <QBuffer>

#include <memory>

#undef FILENAME_MAX

enum
{
    FILENAME_MAX = 255,
    COMMENT_MAX = 4095,
    EXTRA_MAX = 4096
};
/// @cond internal
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
    void restoreOriginalFileName();

    QTextCodec *fileNameCodec;
    QTextCodec *commentCodec;
    char originalFileName[FILENAME_MAX + 1];
    char comment[COMMENT_MAX + 1];
    char extraField[EXTRA_MAX];
    gz_header gzHeader;
};
/// @endcond

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
    return FILENAME_MAX;
}

int QuaGzipDevice::maxCommentLength()
{
    return COMMENT_MAX;
}

bool QuaGzipDevice::headerIsProcessed() const
{
    return d()->gzHeader.done != 0;
}

QTextCodec *QuaGzipDevice::fileNameCodec() const
{
    return d()->fileNameCodec;
}

void QuaGzipDevice::setFileNameCodec(QTextCodec *codec)
{
    Q_ASSERT(codec);
    d()->fileNameCodec = codec;
}

void QuaGzipDevice::setFileNameCodec(const char *codecName)
{
    setFileNameCodec(QTextCodec::codecForName(codecName));
}

QTextCodec *QuaGzipDevice::commentCodec() const
{
    return d()->commentCodec;
}

void QuaGzipDevice::setCommentCodec(QTextCodec *codec)
{
    Q_ASSERT(codec);
    d()->commentCodec = codec;
}

void QuaGzipDevice::setCommentCodec(const char *codecName)
{
    setCommentCodec(QTextCodec::codecForName(codecName));
}

void QuaGzipDevice::restoreOriginalFileName()
{
    d()->restoreOriginalFileName();
}

QString QuaGzipDevice::originalFileName() const
{
    if (headerIsProcessed()) {
        d()->restoreOriginalFileName();
    }

    return d()->fileNameCodec->toUnicode(d()->originalFileName);
}

void QuaGzipDevice::setOriginalFileName(const QString &fileName)
{
    if (!d()->fileNameCodec->canEncode(fileName)) {
        d()->setError(QStringLiteral("Unable to encode original file name"));
        return;
    }

    QByteArray mbcsFileName = d()->fileNameCodec->fromUnicode(fileName);

    int length = mbcsFileName.length();
    if (length > FILENAME_MAX) {
        d()->setError(QStringLiteral(
            "Unable to set more than %1 bytes for original file name.")
                          .arg(FILENAME_MAX));
        return;
    }
    memcpy(d()->originalFileName, mbcsFileName.data(), length);
    d()->originalFileName[length] = 0;
}

QString QuaGzipDevice::comment() const
{
    return d()->commentCodec->toUnicode(d()->comment);
}

void QuaGzipDevice::setComment(const QString &text)
{
    if (!d()->commentCodec->canEncode(text)) {
        d()->setError(QStringLiteral("Unable to encode comment."));
        return;
    }

    auto comment = d()->commentCodec->fromUnicode(text);
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

QuaZExtraField::Map QuaGzipDevice::extraFields() const
{
    auto &header = d()->gzHeader;
    return QuaZExtraField::toMap(header.extra, header.extra_len);
}

void QuaGzipDevice::setExtraFields(const QuaZExtraField::Map &map)
{
    auto &header = d()->gzHeader;

    QuaZExtraField::ResultCode code;
    auto bytes = QuaZExtraField::fromMap(map, &code, header.extra_max);
    if (code == QuaZExtraField::OK) {
        header.extra_len = bytes.length();
        memcpy(header.extra, bytes.data(), bytes.length());
    } else {
        d()->setError(QuaZExtraField::errorString(code));
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
    originalFileName[0] = 0;
    comment[0] = 0;
    gzHeader.extra = reinterpret_cast<Bytef *>(extraField);
    gzHeader.extra_max = EXTRA_MAX;
    gzHeader.name = reinterpret_cast<Bytef *>(originalFileName);
    gzHeader.name_max = FILENAME_MAX;
    gzHeader.comment = reinterpret_cast<Bytef *>(comment);
    gzHeader.comm_max = COMMENT_MAX;

    fileNameCodec = QTextCodec::codecForLocale();
    commentCodec = fileNameCodec;
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
        MAX_WBITS | QuaGzipDevice::GZIP_FLAG, MAX_MEM_LEVEL, strategy));
}

bool QuaGzipDevicePrivate::gzReadHeader()
{
    gzInitHeader();
    originalFileName[FILENAME_MAX] = 0;
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
    restoreOriginalFileName();
}

void QuaGzipDevicePrivate::restoreOriginalFileName()
{
    if (!io)
        return;

    if (0 != originalFileName[0])
        return;

    auto file = qobject_cast<QFileDevice *>(io);
    if (!file)
        return;

    QFileInfo fileInfo(file->fileName());

    if (!fileNameCodec->canEncode(fileInfo.fileName())) {
        return;
    }

    auto fileName = fileNameCodec->fromUnicode(fileInfo.fileName());

    static const char GZ[] = "gz";
    if (0 ==
        fileInfo.suffix().compare(QLatin1String(GZ), Qt::CaseInsensitive)) {
        fileName.resize(fileName.length() - sizeof(GZ));
    }

    int length = fileName.length();
    if (length <= FILENAME_MAX) {
        memcpy(originalFileName, fileName.data(), length);
        originalFileName[length] = 0;
    }
}
