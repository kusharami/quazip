/*
Copyright (C) 2005-2014 Sergey A. Tachenov

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

#include "testquachecksum32.h"

#include "quazip/quaadler32.h"
#include "quazip/quacrc32.h"

#include "qztest.h"

#include <QBuffer>

using ChecksumCalculate = quint32 (*)(const QByteArray &ba, int size);
using ChecksumCalculateDevice = quint32 (*)(QIODevice *io, qint64 size);
using ChecksumCalculateBuffer = quint32 (*)(const void *data, size_t size);
using ChecksumCalculatorCreate = QuaChecksum32 *(*) ();

Q_DECLARE_METATYPE(ChecksumCalculate)
Q_DECLARE_METATYPE(ChecksumCalculateBuffer)
Q_DECLARE_METATYPE(ChecksumCalculateDevice)
Q_DECLARE_METATYPE(ChecksumCalculatorCreate)

static QuaChecksum32 *newCrc32()
{
    return new QuaCrc32;
}

static QuaChecksum32 *newAdler32()
{
    return new QuaAdler32;
}

void TestQuaChecksum32::calculate_data()
{
    QADD_COLUMN(ChecksumCalculate, calc);
    QADD_COLUMN(ChecksumCalculateBuffer, calcBuffer);
    QADD_COLUMN(ChecksumCalculateDevice, calcDevice);
    QADD_COLUMN(QByteArray, data);
    QADD_COLUMN(quint32, expectedChecksum);

    QTest::newRow("crc32 zero")
        << ChecksumCalculate(&zChecksum<QuaCrc32>)
        << ChecksumCalculateBuffer(&zChecksum<QuaCrc32>)
        << ChecksumCalculateDevice(&zChecksum<QuaCrc32>) << QByteArray() << 0u;
    QTest::newRow("crc32") << ChecksumCalculate(&zChecksum<QuaCrc32>)
                           << ChecksumCalculateBuffer(&zChecksum<QuaCrc32>)
                           << ChecksumCalculateDevice(&zChecksum<QuaCrc32>)
                           << QByteArrayLiteral("Wikipedia") << 0xADAAC02Eu;

    QTest::newRow("adler32 zero")
        << ChecksumCalculate(&zChecksum<QuaAdler32>)
        << ChecksumCalculateBuffer(&zChecksum<QuaAdler32>)
        << ChecksumCalculateDevice(&zChecksum<QuaAdler32>) << QByteArray()
        << 1u;
    QTest::newRow("adler32") << ChecksumCalculate(&zChecksum<QuaAdler32>)
                             << ChecksumCalculateBuffer(&zChecksum<QuaAdler32>)
                             << ChecksumCalculateDevice(&zChecksum<QuaAdler32>)
                             << QByteArrayLiteral("Wikipedia") << 0x11E60398u;
}

void TestQuaChecksum32::calculate()
{
    QFETCH(ChecksumCalculate, calc);
    QFETCH(ChecksumCalculateBuffer, calcBuffer);
    QFETCH(ChecksumCalculateDevice, calcDevice);
    QFETCH(QByteArray, data);
    QFETCH(quint32, expectedChecksum);

    // Calculate with QByteArray
    QCOMPARE(calc(data, -1), expectedChecksum);
    QCOMPARE(calc(data, data.length()), expectedChecksum);

    if (!data.isEmpty()) {
        QVERIFY(calc(data, data.length() - 1) != expectedChecksum);
    }

    // Calculate with memory pointer
    QCOMPARE(
        calcBuffer(data.constData(), size_t(data.length())), expectedChecksum);

    // Calculate with io devices
    QBuffer buffer(&data);
    QVERIFY(buffer.open(QBuffer::ReadOnly));
    QCOMPARE(calcDevice(&buffer, -1), expectedChecksum);
    QCOMPARE(buffer.pos(), buffer.size());

    QCOMPARE(calcDevice(&buffer, buffer.size()) != expectedChecksum,
        buffer.size() > 0);

    QVERIFY(buffer.reset());
    QCOMPARE(calcDevice(&buffer, buffer.size()), expectedChecksum);

    if (!data.isEmpty()) {
        buffer.reset();
        QVERIFY(calcDevice(&buffer, data.length() - 1) != expectedChecksum);
    }
}

void TestQuaChecksum32::update_data()
{
    QADD_COLUMN(ChecksumCalculatorCreate, newCalculator);
    QADD_COLUMN(QList<QByteArray>, joinData);
    QADD_COLUMN(quint32, expectedChecksum);

    QList<QByteArray> joinData;
    joinData << QByteArrayLiteral("Wiki");
    joinData << QByteArrayLiteral("pedia");

    QTest::newRow("crc32") << &newCrc32 << joinData << 0xADAAC02Eu;
    QTest::newRow("adler32") << &newAdler32 << joinData << 0x11E60398u;
}

void TestQuaChecksum32::update()
{
    QFETCH(ChecksumCalculatorCreate, newCalculator);
    QFETCH(QList<QByteArray>, joinData);
    QFETCH(quint32, expectedChecksum);

    QScopedPointer<QuaChecksum32> calc(newCalculator());

    quint32 initValue = calc->value();
    quint32 saveValue = initValue;
    // Update with QByteArray
    for (auto &data : joinData) {
        saveValue = calc->value();
        QVERIFY(calc->update(data));
    }
    QCOMPARE(calc->value(), expectedChecksum);

    calc->reset();
    QCOMPARE(calc->value(), initValue);
    // Update with memory pointer
    for (auto &data : joinData) {
        calc->update(data.data(), size_t(data.length()));
    }
    QCOMPARE(calc->value(), expectedChecksum);

    // Update with io device
    QBuffer buffer;
    QVERIFY(buffer.open(QBuffer::ReadWrite));
    for (auto &data : joinData) {
        QCOMPARE(buffer.write(data), qint64(data.length()));
    }
    QVERIFY(buffer.reset());
    calc->reset();
    for (auto &data : joinData) {
        QVERIFY(calc->update(&buffer, data.length()));
    }
    QCOMPARE(calc->value(), expectedChecksum);

    calc->setValue(saveValue);
    QVERIFY(calc->update(joinData.last()));
    QCOMPARE(calc->value(), expectedChecksum);
}
