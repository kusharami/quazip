/*
Copyright (C) 2018 Alexandra Cherdantseva

This file is part of QuaZIP test suite.

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

#include "testquagzipdevice.h"

#include "quazip/quagzipdevice.h"
#include "quazip/private/quaziodeviceprivate.h"
#include "qztest.h"

#include <QTemporaryDir>
#include <QDateTime>
#include <QtTest/QtTest>

#include <zlib.h>

Q_DECLARE_METATYPE(QuaZExtraField::Map)

struct TestQuaGzipDevice::Gzip {
    gzFile file = NULL;

    ~Gzip();
};

void TestQuaGzipDevice::read_data()
{
    QADD_COLUMN(bool, isGzFile);
    QADD_COLUMN(QByteArray, fileName);
    QADD_COLUMN(QByteArray, data);
    QADD_COLUMN(QByteArray, comment);

    QTest::newRow("file") << true << QByteArray("test.txt")
                          << QByteArray("My simple test") << QByteArray();
    QTest::newRow("buffer") << false << QByteArray("test.bin")
                            << QByteArray(16, 'A') << QByteArray("Binary");
}

static QByteArray fromNativeSeparatedText(const QByteArray &text)
{
    QByteArray result = text;
    result.replace('\r', QByteArray());
    return result;
}

static QByteArray toNativeSeparatedText(const QByteArray &text)
{
    auto result = fromNativeSeparatedText(text);
#ifdef Q_OS_WIN
    result.replace('\n', "\r\n");
#endif
    return result;
}

void TestQuaGzipDevice::read()
{
    QFETCH(bool, isGzFile);
    QFETCH(QByteArray, fileName);
    QFETCH(QByteArray, data);
    QFETCH(QByteArray, comment);

    QScopedPointer<QTemporaryDir> tempDir;
    QScopedPointer<QIODevice> ioDevice;
    if (isGzFile) {
        tempDir.reset(new QTemporaryDir);
        auto gzFilePath = QDir(tempDir->path()).filePath(fileName + ".gz");
        auto localFilePath = gzFilePath.toLocal8Bit();

        {
            Gzip gzip;
            gzip.file = gzopen(localFilePath.data(), "wb");
            gzwrite(gzip.file, data.data(), data.length());
        }

        comment.clear();
        ioDevice.reset(new QFile(gzFilePath));
    } else {
        auto buffer = new QBuffer;
        buffer->setData(QByteArray(256, 0));
        z_stream zouts;
        zouts.zalloc = Z_NULL;
        zouts.zfree = Z_NULL;
        zouts.opaque = Z_NULL;
        gz_header gzHeader;
        memset(&gzHeader, 0, sizeof(gzHeader));
        gzHeader.os = 255;
        gzHeader.name = reinterpret_cast<Bytef *>(fileName.data());
        gzHeader.comment = reinterpret_cast<Bytef *>(comment.data());
        deflateInit2(&zouts, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
            MAX_WBITS | GZIP_FLAG, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
        deflateSetHeader(&zouts, &gzHeader);
        zouts.next_in = reinterpret_cast<Bytef *>(data.data());
        zouts.avail_in = data.length();
        zouts.next_out = reinterpret_cast<Bytef *>(buffer->buffer().data());
        zouts.avail_out = buffer->buffer().size();
        deflate(&zouts, Z_FINISH);
        deflateEnd(&zouts);

        ioDevice.reset(buffer);
    }

    QuaGzipDevice gzDevice(ioDevice.data());

    QVERIFY(!gzDevice.open(QIODevice::ReadWrite));
    QVERIFY(!gzDevice.open(QIODevice::ReadOnly | QIODevice::Append));
    QVERIFY(!gzDevice.open(QIODevice::ReadOnly | QIODevice::Truncate));
    QVERIFY(gzDevice.open(QIODevice::ReadOnly));
    QVERIFY(gzDevice.isOpen());
    QVERIFY(gzDevice.isReadable());
    QVERIFY(!gzDevice.isWritable());
    QVERIFY(!gzDevice.isSequential());
    QVERIFY(!gzDevice.hasError());
    QVERIFY(!gzDevice.atEnd());
    QCOMPARE(gzDevice.bytesAvailable(), qint64(data.length()));
    QCOMPARE(gzDevice.size(), qint64(data.length()));
    QVERIFY(!gzDevice.hasError());
    QCOMPARE(gzDevice.peek(data.length() + 1), data);
    QVERIFY(!gzDevice.hasError());
    QVERIFY(!gzDevice.atEnd());
    QCOMPARE(gzDevice.bytesAvailable(), qint64(data.length()));
    QCOMPARE(gzDevice.pos(), qint64(0));
    QCOMPARE(gzDevice.size(), qint64(data.length()));
    QCOMPARE(gzDevice.read(data.length()), data);
    QCOMPARE(gzDevice.pos(), qint64(data.length()));
    QVERIFY(!gzDevice.hasError());
    QCOMPARE(gzDevice.size(), qint64(data.length()));
    QCOMPARE(gzDevice.bytesAvailable(), 0);
    QVERIFY(gzDevice.atEnd());
    QVERIFY(gzDevice.headerIsProcessed());
    QCOMPARE(gzDevice.comment(), QString::fromLatin1(comment));
    QCOMPARE(gzDevice.originalFileName(), QString::fromLatin1(fileName));
    gzDevice.close();
    QVERIFY(!gzDevice.hasError());
    QVERIFY(!gzDevice.isOpen());

    ioDevice->close();

    QVERIFY(gzDevice.open(QIODevice::WriteOnly));
    QVERIFY(gzDevice.isOpen());
    char badBuf[1];
    QCOMPARE(gzDevice.read(badBuf, 1), qint64(-1));
    QVERIFY(!gzDevice.errorString().isEmpty());
}

void TestQuaGzipDevice::write_data()
{
    QADD_COLUMN(QByteArray, fileName);
    QADD_COLUMN(QByteArray, data);
    QADD_COLUMN(QByteArray, comment);
    QADD_COLUMN(QuaZExtraField::Map, extra);
    QADD_COLUMN(int, compressionLevel);
    QADD_COLUMN(bool, isText);

    {
        QuaZExtraField::Map extra;
        extra[QuaZExtraField::Key("T0")] = QByteArrayLiteral("Test me");

        QTest::newRow("text")
            << QByteArray("test.txt") << QByteArray("Test write\ntext file")
            << QByteArray("This is a text file") << extra << Z_BEST_COMPRESSION
            << true;
    }

    {
        QuaZExtraField::Map extra;
        extra[QuaZExtraField::Key("aA")] = QByteArrayLiteral("I am aa");
        extra[QuaZExtraField::Key("bB")] =
            QByteArrayLiteral("I am not aa, I am bB");

        QTest::newRow("binary")
            << QByteArray("test.bin") << QByteArray(8, 'b') + QByteArray(3, 'c')
            << QByteArray("This is binary file") << extra << Z_BEST_SPEED
            << false;
    }

    QTest::newRow("empty") << QByteArray("empty.dat") << QByteArray()
                           << QByteArray() << QuaZExtraField::Map()
                           << Z_NO_COMPRESSION << false;
}

void TestQuaGzipDevice::write()
{
    QFETCH(QByteArray, fileName);
    QFETCH(QByteArray, data);
    QFETCH(QByteArray, comment);
    QFETCH(QuaZExtraField::Map, extra);
    QFETCH(int, compressionLevel);
    QFETCH(bool, isText);

    quint32 savedTime = quint32(QDateTime::currentMSecsSinceEpoch() / 1000);

    QTemporaryDir tempDir;
    auto gzFilePath = QDir(tempDir.path()).filePath(fileName + ".gz");
    QuaGzipDevice gzDevice;

    QFile fileGZ(gzFilePath);
    QIODevice::OpenMode writeMode = QIODevice::WriteOnly;
    if (isText)
        writeMode |= QIODevice::Text;

    gzDevice.setCompressionLevel(compressionLevel);
    QCOMPARE(gzDevice.compressionLevel(), compressionLevel);
    gzDevice.setIODevice(&fileGZ);
    QCOMPARE(gzDevice.ioDevice(), &fileGZ);
    gzDevice.setOriginalFileName(fileName);
    QCOMPARE(gzDevice.originalFileName(), fileName);
    gzDevice.setComment(comment);
    QCOMPARE(gzDevice.comment(), comment);
    gzDevice.setExtraFields(extra);
    QCOMPARE(gzDevice.extraFields(), extra);
    gzDevice.setModificationTime(savedTime);
    QCOMPARE(gzDevice.modificationTime(), savedTime);
    QVERIFY(gzDevice.open(writeMode));
    QVERIFY(gzDevice.isOpen());
    QVERIFY(gzDevice.isWritable());
    QVERIFY(!gzDevice.isReadable());
    QVERIFY(gzDevice.isSequential());
    QCOMPARE(gzDevice.isTextModeEnabled(), isText);
    QVERIFY(!fileGZ.isTextModeEnabled());
    QVERIFY(gzDevice.openMode() & QIODevice::Truncate);
    QVERIFY(fileGZ.openMode() & QIODevice::Truncate);
    QVERIFY(!gzDevice.hasError());
    QCOMPARE(gzDevice.size(), qint64(0));
    QCOMPARE(gzDevice.write(data), qint64(data.length()));
    auto expectedUncompressedData = isText ? toNativeSeparatedText(data) : data;
    QCOMPARE(gzDevice.size(), expectedUncompressedData.length());
    QVERIFY(!gzDevice.hasError());
    fileGZ.close();
    QVERIFY(!gzDevice.hasError());
    QVERIFY2(!gzDevice.isOpen(),
        "gzDevice should be closed after dependent device is closed");

    QIODevice::OpenMode readMode = QIODevice::ReadOnly;
    if (isText)
        readMode |= QIODevice::Text;
    QVERIFY(fileGZ.open(readMode));
    if (isText) {
        QVERIFY(!gzDevice.open(readMode));
        QVERIFY2(gzDevice.hasError(),
            "Should be unable to decompress file opened in text mode.");
        fileGZ.close();
        QVERIFY(fileGZ.open(QIODevice::ReadOnly));
    }
    QVERIFY(gzDevice.open(readMode));

    expectedUncompressedData = isText ? fromNativeSeparatedText(data) : data;
    QCOMPARE(gzDevice.readAll(), expectedUncompressedData);
    QVERIFY(gzDevice.headerIsProcessed());
    QCOMPARE(gzDevice.originalFileName(), QString::fromLatin1(fileName));
    QCOMPARE(gzDevice.comment(), QString::fromLatin1(comment));
    QCOMPARE(gzDevice.extraFields(), extra);
    QCOMPARE(gzDevice.modificationTime(), savedTime);

    gzDevice.close();

    fileGZ.reset();
    auto compressedData = fileGZ.readAll();
    fileGZ.close();

    z_stream zins;
    zins.zalloc = Z_NULL;
    zins.zfree = Z_NULL;
    zins.opaque = Z_NULL;
    inflateInit2(&zins, GZIP_FLAG);

    gz_header gzHeader;
    memset(&gzHeader, 0, sizeof(gzHeader));
    gzHeader.os = 255;
    Byte temp_name[256];
    gzHeader.name = temp_name;
    gzHeader.name_max = fileName.length();
    gzHeader.name[gzHeader.name_max] = 0;
    Byte temp_comment[256];
    gzHeader.comment = temp_comment;
    gzHeader.comm_max = comment.length();
    gzHeader.comment[gzHeader.comm_max] = 0;
    Byte temp_extra[256];
    gzHeader.extra = temp_extra;
    gzHeader.extra_max = sizeof(temp_extra);

    inflateGetHeader(&zins, &gzHeader);

    expectedUncompressedData = isText ? toNativeSeparatedText(data) : data;
    QByteArray uncompressedData(expectedUncompressedData.length(), 0);
    zins.next_in = reinterpret_cast<Bytef *>(compressedData.data());
    zins.avail_in = uInt(compressedData.length());

    zins.next_out = reinterpret_cast<Bytef *>(uncompressedData.data());
    zins.avail_out = uInt(uncompressedData.length());

    int inflateResult = inflate(&zins, Z_FINISH);
    inflateEnd(&zins);
    QCOMPARE(inflateResult, Z_STREAM_END);

    QCOMPARE(zins.avail_out, uInt(0));
    QCOMPARE(uncompressedData, expectedUncompressedData);
    QVERIFY(gzHeader.done);
    QCOMPARE(QByteArray(reinterpret_cast<char *>(temp_name)), fileName);
    QCOMPARE(QByteArray(reinterpret_cast<char *>(temp_comment)), comment);
    QVERIFY(extra.isEmpty() || gzHeader.extra_len > 0);
    QCOMPARE(gzHeader.text != 0, isText);
    QCOMPARE(gzHeader.time, uLong(savedTime));

    auto e = gzHeader.extra;

    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
        auto &key = it.key();
        auto &value = it.value();
        QCOMPARE(e[0], quint8(key));
        QCOMPARE(e[1], quint8(key >> 8));
        QCOMPARE(e[2], quint8(value.length()));
        QCOMPARE(e[3], quint8(value.length() >> 8));

        QCOMPARE(QByteArray::fromRawData(
                     reinterpret_cast<char *>(&e[4]), value.length()),
            value);

        e += 4 + value.length();
    }

    QByteArray expectedCompressedData(compressedData.length(), 0);
    z_stream zouts;
    zouts.zalloc = Z_NULL;
    zouts.zfree = Z_NULL;
    zouts.opaque = Z_NULL;
    deflateInit2(&zouts, compressionLevel, Z_DEFLATED, MAX_WBITS | GZIP_FLAG,
        MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    deflateSetHeader(&zouts, &gzHeader);
    zouts.next_in = reinterpret_cast<Bytef *>(expectedUncompressedData.data());
    zouts.avail_in = expectedUncompressedData.length();
    zouts.next_out = reinterpret_cast<Bytef *>(expectedCompressedData.data());
    zouts.avail_out = expectedCompressedData.length();
    deflate(&zouts, Z_FINISH);
    deflateEnd(&zouts);

    QCOMPARE(compressedData, expectedCompressedData);

    auto localFilePath = gzFilePath.toLocal8Bit();
    Gzip gzip;
    gzip.file = gzopen(localFilePath.data(), "rb");
    uncompressedData = QByteArray(expectedUncompressedData.length(), 0);
    QCOMPARE(gzread(gzip.file, uncompressedData.data(),
                 qMax(1, uncompressedData.length())),
        expectedUncompressedData.length());
    QCOMPARE(uncompressedData, expectedUncompressedData);
}

TestQuaGzipDevice::Gzip::~Gzip()
{
    if (file) {
        gzclose(file);
    }
}
