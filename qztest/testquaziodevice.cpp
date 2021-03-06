/*
Copyright (C) 2005-2014 Sergey A. Tachenov
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

#include "testquaziodevice.h"
#include "quazip/quaziodevice.h"

#include <QBuffer>
#include <QByteArray>
#include <QtTest/QtTest>

#include <zlib.h>

void TestQuaZIODevice::read()
{
    QByteArray buf(256, 0);
    z_stream zouts;
    zouts.zalloc = Z_NULL;
    zouts.zfree = Z_NULL;
    zouts.opaque = Z_NULL;
    deflateInit(&zouts, Z_DEFAULT_COMPRESSION);
    zouts.next_in = reinterpret_cast<const Bytef *>("test");
    zouts.avail_in = 4;
    zouts.next_out = reinterpret_cast<Bytef *>(buf.data());
    zouts.avail_out = buf.size();
    deflate(&zouts, Z_FINISH);
    deflateEnd(&zouts);
    QBuffer testBuffer(&buf);
    QuaZIODevice testDevice(&testBuffer);
    testBuffer.open(QIODevice::ReadOnly);
    QVERIFY(!testDevice.open(QIODevice::WriteOnly));
    QVERIFY(testDevice.hasError());
    QVERIFY(!testDevice.open(QIODevice::Append));
    QVERIFY(testDevice.hasError());
    QVERIFY(!testDevice.open(QIODevice::ReadWrite));
    QVERIFY(testDevice.hasError());
    QVERIFY(!testDevice.open(QIODevice::ReadOnly | QIODevice::Append));
    QVERIFY(testDevice.hasError());
    QVERIFY(!testDevice.open(QIODevice::ReadOnly | QIODevice::Truncate));
    QVERIFY(testDevice.hasError());
    QVERIFY(testDevice.open(QIODevice::ReadOnly));
    QVERIFY(!testDevice.hasError());
    QVERIFY(testDevice.isOpen());
    QVERIFY(testDevice.isReadable());
    QVERIFY(!testDevice.isWritable());
    QVERIFY(!testDevice.isSequential());
    char outBuf[5];
    QCOMPARE(testDevice.peek(outBuf, 5), qint64(4));
    QCOMPARE(QByteArray(outBuf, 4), QByteArrayLiteral("test"));
    QCOMPARE(testDevice.pos(), qint64(0));
    QCOMPARE(testDevice.size(), qint64(4));
    QCOMPARE(testDevice.bytesAvailable(), qint64(4));
    QCOMPARE(testDevice.readAll(), QByteArrayLiteral("test"));
    QCOMPARE(testDevice.bytesAvailable(), qint64(0));
    QCOMPARE(testDevice.size(), qint64(4));
    QCOMPARE(testDevice.pos(), qint64(4));
    QVERIFY(testDevice.atEnd());
    testDevice.close();
    QVERIFY(!testDevice.isOpen());
    QVERIFY(!testDevice.hasError());
}

void TestQuaZIODevice::readMany()
{
    QByteArray buf(256, 0);
    z_stream zouts;
    zouts.zalloc = Z_NULL;
    zouts.zfree = Z_NULL;
    zouts.opaque = Z_NULL;
    deflateInit(&zouts, Z_BEST_COMPRESSION);
    zouts.next_in = reinterpret_cast<const Bytef *>("tatatest");
    zouts.avail_in = 8;
    zouts.next_out = reinterpret_cast<Bytef *>(buf.data());
    zouts.avail_out = buf.size();
    deflate(&zouts, Z_FINISH);
    deflateEnd(&zouts);
    QBuffer testBuffer(&buf);
    testBuffer.open(QIODevice::ReadWrite);
    QuaZIODevice testDevice(&testBuffer);
    QVERIFY(!testDevice.open(QIODevice::Append));
    QVERIFY(testDevice.hasError());
    QVERIFY(!testDevice.open(QIODevice::ReadWrite));
    QVERIFY(testDevice.hasError());
    QVERIFY(!testDevice.open(QIODevice::ReadOnly | QIODevice::Append));
    QVERIFY(testDevice.hasError());
    QVERIFY(!testDevice.open(QIODevice::ReadOnly | QIODevice::Truncate));
    QVERIFY(testDevice.hasError());
    QVERIFY(testDevice.open(QIODevice::ReadOnly));
    QVERIFY(!testDevice.hasError());
    QVERIFY(testDevice.isOpen());
    QVERIFY(testDevice.isReadable());
    QVERIFY(!testDevice.isWritable());
    QVERIFY(!testDevice.isSequential());
    QCOMPARE(testDevice.size(), qint64(8));
    QVERIFY(!testDevice.atEnd());
    QVERIFY(!testDevice.hasError());
    char outBuf[4];
    QCOMPARE(testDevice.peek(outBuf, 4), qint64(4));
    QCOMPARE(QByteArray(outBuf, 4), QByteArrayLiteral("tata"));
    QCOMPARE(testDevice.read(outBuf, 4), qint64(4));
    QCOMPARE(QByteArray(outBuf, 4), QByteArrayLiteral("tata"));
    QCOMPARE(testDevice.pos(), qint64(4));
    QVERIFY(!testDevice.hasError());
    QVERIFY(!testDevice.atEnd());
    QCOMPARE(testDevice.bytesAvailable(), qint64(4));
    auto bytes = testDevice.read(4);
    QCOMPARE(bytes, QByteArrayLiteral("test"));
    QCOMPARE(bytes.size(), 4);
    QVERIFY(!testDevice.hasError());
    QCOMPARE(testDevice.bytesAvailable(), qint64(0));
    QVERIFY(testDevice.atEnd());
    QCOMPARE(testDevice.size(), qint64(8));
    testDevice.close();
    QVERIFY(!testDevice.hasError());
    QVERIFY(!testDevice.isOpen());
}

void TestQuaZIODevice::write_data()
{
    QTest::addColumn<bool>("truncate");
    QTest::addColumn<QByteArray>("data");

    QTest::addRow("truncate_false") << false << QByteArrayLiteral("test");
    QTest::addRow("truncate_true") << true << QByteArrayLiteral("bigTest");
    QTest::addRow("empty") << true << QByteArray();
}

void TestQuaZIODevice::write()
{
    QFETCH(bool, truncate);
    QFETCH(QByteArray, data);

    QByteArray buf(256, 0);
    QBuffer testBuffer(&buf);

    QIODevice::OpenMode writeMode = QIODevice::WriteOnly;

    if (truncate) {
        writeMode |= QIODevice::Truncate;
    } else {
        testBuffer.open(writeMode);
    }

    QuaZIODevice testDevice(&testBuffer);
    QCOMPARE(testDevice.getIODevice(), &testBuffer);
    QVERIFY(!testDevice.open(QIODevice::ReadWrite));
    QVERIFY(testDevice.hasError());
    QVERIFY(!testDevice.open(QIODevice::Append));
    QVERIFY(testDevice.hasError());
    QVERIFY(testDevice.open(writeMode));
    QVERIFY(testDevice.isOpen());
    QVERIFY(!testDevice.isReadable());
    QVERIFY(testDevice.isWritable());
    QVERIFY(testDevice.isSequential());
    QVERIFY(testDevice.openMode() & QIODevice::Truncate);
    QCOMPARE(0 != (testBuffer.openMode() & QIODevice::Truncate), truncate);
    QCOMPARE(testDevice.size(), qint64(0));
    QVERIFY(!testDevice.hasError());
    QCOMPARE(testDevice.write(data), qint64(data.length()));
    QCOMPARE(testDevice.size(), qint64(data.length()));
    QVERIFY(!testDevice.hasError());
    testDevice.close();
    QVERIFY(!testDevice.hasError());
    QVERIFY(!testDevice.isOpen());
    z_stream zins;
    zins.zalloc = Z_NULL;
    zins.zfree = Z_NULL;
    zins.opaque = Z_NULL;
    inflateInit(&zins);
    zins.next_in = reinterpret_cast<Bytef *>(buf.data());
    zins.avail_in = uInt(testBuffer.pos());
    QByteArray outBuf(data.length(), 0);
    zins.next_out = reinterpret_cast<Bytef *>(outBuf.data());
    zins.avail_out = outBuf.length();
    int inflateResult = inflate(&zins, Z_FINISH);
    inflateEnd(&zins);
    QCOMPARE(inflateResult, Z_STREAM_END);
    QCOMPARE(zins.avail_out, uInt(0));
    QCOMPARE(outBuf, data);
}
