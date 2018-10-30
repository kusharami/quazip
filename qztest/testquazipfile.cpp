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

#include "testquazipfile.h"

#include "qztest.h"

#include "quazip/JlCompress.h"
#include "quazip/quazipfile.h"
#include "quazip/quazip.h"
#include "quazip/zip.h"
#include "quazip/unzip.h"

#include <QFile>
#include <QString>
#include <QStringList>

#include <QtTest/QtTest>

void TestQuaZipFile::zipUnzip_data()
{
    QTest::addColumn<QStringList>("fileNames");
    QTest::addColumn<QByteArray>("filePathCodec");
    QTest::addColumn<QByteArray>("password");
    QTest::addColumn<bool>("zip64");
    QTest::addColumn<int>("size");
    QTest::newRow("simple") << "simple.zip"
                            << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << QByteArray() << QByteArray() << false << -1;
    QTest::newRow("Cyrillic")
        << "cyrillic.zip"
        << (QStringList() << QString::fromUtf8(
                "русское имя файла с пробелами.txt"))
        << QByteArray("IBM866") << QByteArray() << false << -1;
    QTest::newRow("password")
        << "password.zip" << (QStringList() << "test.txt") << QByteArray()
        << QByteArray("PassPass") << false << -1;
    QTest::newRow("zip64") << "zip64.zip" << (QStringList() << "test64.txt")
                           << QByteArray() << QByteArray() << true << -1;
    QTest::newRow("large enough to flush")
        << "flush.zip" << (QStringList() << "flush.txt") << QByteArray()
        << QByteArray() << true << 65536 * 2;
}

void TestQuaZipFile::zipUnzip()
{
    QFETCH(QString, zipName);
    QFETCH(QStringList, fileNames);
    QFETCH(QByteArray, filePathCodec);
    QFETCH(QByteArray, password);
    QFETCH(bool, zip64);
    QFETCH(int, size);
    QFile testFile(zipName);
    if (testFile.exists()) {
        if (!testFile.remove()) {
            QFAIL("Couldn't remove existing archive to create a new one");
        }
    }
    if (!createTestFiles(fileNames, size)) {
        QFAIL("Couldn't create test files for zipping");
    }
    QuaZip testZip(&testFile);
    testZip.setZip64Enabled(zip64);
    if (!filePathCodec.isEmpty())
        testZip.setFilePathCodec(filePathCodec);
    QVERIFY(testZip.open(QuaZip::mdCreate));
    QString comment = "Test comment";
    testZip.setComment(comment);
    foreach (QString fileName, fileNames) {
        QFile inFile("tmp/" + fileName);
        if (!inFile.open(QIODevice::ReadOnly)) {
            qDebug("File name: %s", fileName.toUtf8().constData());
            QFAIL("Couldn't open input file");
        }
        QuaZipFile outFile(&testZip);
        QVERIFY(outFile.open(QIODevice::WriteOnly,
            QuaZipNewInfo(fileName, inFile.fileName()),
            password.isEmpty() ? NULL : password.constData()));
        for (qint64 pos = 0, len = inFile.size(); pos < len;) {
            char buf[4096];
            qint64 readSize = qMin(static_cast<qint64>(4096), len - pos);
            qint64 l;
            if ((l = inFile.read(buf, readSize)) != readSize) {
                qDebug("Reading %ld bytes from %s at %ld returned %ld",
                    static_cast<long>(readSize), fileName.toUtf8().constData(),
                    static_cast<long>(pos), static_cast<long>(l));
                QFAIL("Read failure");
            }
            QVERIFY(outFile.write(buf, readSize));
            pos += readSize;
        }
        inFile.close();
        outFile.close();
        QCOMPARE(outFile.getZipError(), ZIP_OK);
    }
    testZip.close();
    QCOMPARE(testZip.getZipError(), ZIP_OK);
    // now test unzip
    QuaZip testUnzip(&testFile);
    if (!filePathCodec.isEmpty())
        testUnzip.setFilePathCodec(filePathCodec);
    QVERIFY(testUnzip.open(QuaZip::mdUnzip));
    QCOMPARE(testUnzip.getComment(), comment);
    QVERIFY(testUnzip.goToFirstFile());
    foreach (QString fileName, fileNames) {
        QuaZipFileInfo info;
        QVERIFY(testUnzip.getCurrentFileInfo(&info));
        QCOMPARE(info.name, fileName);
        QCOMPARE(info.isEncrypted(), !password.isEmpty());
        QFile original("tmp/" + fileName);
        QVERIFY(original.open(QIODevice::ReadOnly));
        QuaZipFile archived(&testUnzip);
        QVERIFY(archived.open(QIODevice::ReadOnly,
            password.isEmpty() ? NULL : password.constData()));
        QByteArray originalData = original.readAll();
        QByteArray archivedData = archived.readAll();
        QCOMPARE(archivedData, originalData);
        testUnzip.goToNextFile();
    }
    if (!password.isEmpty()) {
        QVERIFY(testUnzip.goToFirstFile());
        QuaZipFileInfo info;
        QVERIFY(testUnzip.getCurrentFileInfo(&info));
        QFile original("tmp/" + info.name);
        QVERIFY(original.open(QIODevice::ReadOnly));
        QuaZipFile archived(&testUnzip);
        QVERIFY(archived.open(QIODevice::ReadOnly, "WrongPassword"));
        QByteArray originalData = original.readAll();
        QByteArray archivedData = archived.readAll();
        QVERIFY(archivedData != originalData);
    }
    testUnzip.close();
    QCOMPARE(testUnzip.getZipError(), UNZ_OK);
    // clean up
    removeTestFiles(fileNames);
    testFile.remove();
}

void TestQuaZipFile::bytesAvailable_data()
{
    QTest::addColumn<QString>("zipName");
    QTest::addColumn<QStringList>("fileNames");
    QTest::addColumn<int>("size");
    QTest::newRow("simple") << "test.zip"
                            << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << -1;
    QTest::newRow("large enough to flush")
        << "flush.zip" << (QStringList() << "test.txt") << 65536 * 4;
}

void TestQuaZipFile::bytesAvailable()
{
    QFETCH(QString, zipName);
    QFETCH(QStringList, fileNames);
    QFETCH(int, size);
    QDir curDir;
    if (!createTestFiles(fileNames, size)) {
        QFAIL("Couldn't create test files");
    }
    if (!JlCompress::compressDir(zipName, "tmp")) {
        QFAIL("Couldn't create test archive");
    }
    QuaZip testZip(zipName);
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    foreach (QString fileName, fileNames) {
        QFileInfo fileInfo("tmp/" + fileName);
        QVERIFY(testZip.setCurrentFile(fileName));
        QuaZipFile zipFile(&testZip);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        QCOMPARE(zipFile.size(), fileInfo.size());
    }
    removeTestFiles(fileNames);
    testZip.close();
    curDir.remove(zipName);
}

void TestQuaZipFile::getZip()
{
    QuaZip testZip;
    QuaZipFile f1(&testZip);
    QCOMPARE(f1.zip(), &testZip);
    QuaZipFile f2("doesntexist.zip", "someFile");
    QCOMPARE(f2.zip(), static_cast<QuaZip *>(NULL));
    f2.setZip(&testZip);
    QCOMPARE(f2.zip(), &testZip);
}

void TestQuaZipFile::setZipFilePath()
{
    QString testFileName = "testZipName.txt";
    QString testZipName = "testZipName.zip";
    QVERIFY(createTestFiles(QStringList() << testFileName));
    QVERIFY(createTestArchive(testZipName, QStringList() << testFileName));
    QuaZipFile testFile;
    testFile.setZipFilePath(testZipName);
    QCOMPARE(testFile.zipFilePath(), testZipName);
    testFile.setFilePath(testFileName);
    QVERIFY(testFile.open(QIODevice::ReadOnly));
    testFile.close();
    removeTestFiles(QStringList() << testFileName);
    QDir curDir;
    curDir.remove(testZipName);
}

void TestQuaZipFile::getFileInfo()
{
    QuaZipFileInfo fileInfo;
    QString testFileName = "testZipName.txt";
    QStringList testFiles;
    testFiles << testFileName;
    QString testZipName = "testZipName.zip";
    QVERIFY(createTestFiles(testFiles));
    QVERIFY(createTestArchive(testZipName, testFiles));
    QuaZipFile testFile;
    testFile.setZipFilePath(testZipName);
    testFile.setFilePath(testFileName);
    QVERIFY(testFile.open(QIODevice::ReadOnly));
    QVERIFY(testFile.getFileInfo(&fileInfo));
    testFile.close();
    removeTestFiles(testFiles);
    QDir curDir;
    curDir.remove(testZipName);
}

void TestQuaZipFile::setFileName()
{
    QString testFileName = "testZipName.txt";
    QString testZipName = "testZipName.zip";
    QVERIFY(createTestFiles(QStringList() << testFileName));
    QVERIFY(createTestArchive(testZipName, QStringList() << testFileName));
    QuaZipFile testFile(testZipName);
    testFile.setFilePath(testFileName.toUpper());
#ifdef Q_OS_WIN
    QVERIFY(testFile.open(QIODevice::ReadOnly));
    testFile.close();
#else
    QVERIFY(!testFile.open(QIODevice::ReadOnly));
#endif
    testFile.setFilePath(testFileName.toUpper(), QuaZip::csInsensitive);
    QCOMPARE(testFile.caseSensitivity(), QuaZip::csInsensitive);
    QVERIFY(testFile.open(QIODevice::ReadOnly));
    QCOMPARE(testFile.actualFilePath(), testFileName);
    testFile.close();
    testFile.setFilePath(testFileName.toUpper(), QuaZip::csSensitive);
    QCOMPARE(testFile.filePath(), testFileName.toUpper());
    QCOMPARE(testFile.actualFilePath(), QString());
    QVERIFY(!testFile.open(QIODevice::ReadOnly));
    testFile.setFilePath(testFileName);
    removeTestFiles(QStringList() << testFileName);
    QDir curDir;
    curDir.remove(testZipName);
}

void TestQuaZipFile::constructorDestructor()
{
    // Just test that all constructors and destructors are available.
    // (So there are none that are declared but not defined.)
    QuaZip testZip;
    QuaZipFile *f1 = new QuaZipFile();
    delete f1; // test D0 destructor
    QObject parent;
    QuaZipFile f2(&testZip, &parent);
    QuaZipFile f3(&parent);
    QuaZipFile f4("zipName.zip");
    QuaZipFile f5("zipName.zip", "fileName.txt", QuaZip::csDefault, &parent);
}

void TestQuaZipFile::setFileAttrs()
{
    QuaZip testZip("setFileAttrs.zip");
    QVERIFY(testZip.open(QuaZip::mdCreate));
    QuaZipFile zipFile(&testZip);
    QuaZipNewInfo newInfo("testPerm.txt");
    newInfo.setPermissions(QFile::ReadOwner);
    QVERIFY(zipFile.open(QIODevice::WriteOnly, newInfo));
    zipFile.close();
    QString fileTestAttr = "testAttr.txt";
    QStringList fileNames;
    fileNames << fileTestAttr;
    QVERIFY(createTestFiles(fileNames));
    newInfo.name = fileTestAttr;
    newInfo.setFileDateTime("tmp/" + fileTestAttr);
    newInfo.setFilePermissions("tmp/" + fileTestAttr);
    QVERIFY(zipFile.open(QIODevice::WriteOnly, newInfo));
    zipFile.close();
    testZip.close();
    QuaZipFileInfo info;
    {
        QuaZipFile readFilePerm("setFileAttrs.zip", "testPerm.txt");
        QVERIFY(readFilePerm.open(QIODevice::ReadOnly));
        QVERIFY(readFilePerm.getFileInfo(&info));
        QCOMPARE(info.getPermissions(), QFile::ReadOwner);
        readFilePerm.close();
    }
    {
        QuaZipFile readFileAttrs("setFileAttrs.zip", "testAttr.txt");
        QVERIFY(readFileAttrs.open(QIODevice::ReadOnly));
        QVERIFY(readFileAttrs.getFileInfo(&info));
        QFileInfo srcInfo("tmp/" + fileTestAttr);
        QFile::Permissions usedPermissions = QFile::WriteOwner |
            QFile::ReadOwner | QFile::ExeOwner | QFile::WriteGroup |
            QFile::ReadGroup | QFile::ExeGroup | QFile::WriteOther |
            QFile::ReadOther | QFile::ExeOther;
        QCOMPARE(info.getPermissions() & usedPermissions,
            srcInfo.permissions() & usedPermissions);
        // I really hope Qt 6 will use quint64 for time_t!
        quint64 newTime = info.dateTime.toTime_t();
        quint64 oldTime = srcInfo.lastModified().toTime_t();
        // ZIP uses weird format with 2 second precision
        QCOMPARE(newTime / 2, oldTime / 2);
        readFileAttrs.close();
    }
    removeTestFiles(fileNames);
    QDir().remove(testZip.zipFilePath());
}

void TestQuaZipFile::largeFile()
{
    QDir curDir;
    QVERIFY(curDir.mkpath("tmp"));
    QFile fakeLargeFile("tmp/large.zip");
    QVERIFY(fakeLargeFile.open(QIODevice::WriteOnly));
    QDataStream ds(&fakeLargeFile);
    ds.setByteOrder(QDataStream::LittleEndian);
    QList<qint64> localOffsets;
    const int numFiles = 2; // name fixed to 5 bytes, so MAX 10 FILES!!!
    for (int i = 0; i < numFiles; ++i) {
        localOffsets.append(fakeLargeFile.pos());
        QBuffer extra;
        extra.open(QIODevice::WriteOnly);
        QDataStream es(&extra);
        es.setByteOrder(QDataStream::LittleEndian);
        // prepare extra
        es << static_cast<quint16>(0x0001u); // zip64
        es << static_cast<quint16>(16); // extra data size
        es << static_cast<quint64>(0); // uncompressed size
        es << static_cast<quint64>(0); // compressed size
        // now the local header
        ds << static_cast<quint32>(0x04034b50u); // local magic
        ds << static_cast<quint16>(45); // version needed
        ds << static_cast<quint16>(0); // flags
        ds << static_cast<quint16>(0); // method
        ds << static_cast<quint16>(0); // time 00:00:00
        ds << static_cast<quint16>(0x21); // date 1980-01-01
        ds << static_cast<quint32>(0); // CRC-32
        ds << static_cast<quint32>(0xFFFFFFFFu); // compressed size
        ds << static_cast<quint32>(0xFFFFFFFFu); // uncompressed size
        ds << static_cast<quint16>(5); // name length
        ds << static_cast<quint16>(extra.size()); // extra length
        ds.writeRawData("file", 4); // name
        ds << static_cast<qint8>('0' + i); // name (contd.)
        ds.writeRawData(extra.buffer(), uint(extra.size()));
    }
    // central dir:
    qint64 centralStart = fakeLargeFile.pos();
    for (int i = 0; i < numFiles; ++i) {
        QBuffer extra;
        extra.open(QIODevice::WriteOnly);
        QDataStream es(&extra);
        es.setByteOrder(QDataStream::LittleEndian);
        // prepare extra
        es << static_cast<quint16>(0x0001u); // zip64
        es << static_cast<quint16>(24); // extra data size
        es << static_cast<quint64>(0); // uncompressed size
        es << static_cast<quint64>(0); // compressed size
        es << static_cast<quint64>(localOffsets[i]);
        // now the central dir header
        ds << static_cast<quint32>(0x02014b50u); // central magic
        ds << static_cast<quint16>(45); // version made by
        ds << static_cast<quint16>(45); // version needed
        ds << static_cast<quint16>(0); // flags
        ds << static_cast<quint16>(0); // method
        ds << static_cast<quint16>(0); // time 00:00:00
        ds << static_cast<quint16>(0x21); // date 1980-01-01
        ds << static_cast<quint32>(0); // CRC-32
        ds << static_cast<quint32>(0xFFFFFFFFu); // compressed size
        ds << static_cast<quint32>(0xFFFFFFFFu); // uncompressed size
        ds << static_cast<quint16>(5); // name length
        ds << static_cast<quint16>(extra.size()); // extra length
        ds << static_cast<quint16>(0); // comment length
        ds << static_cast<quint16>(0); // disk number
        ds << static_cast<quint16>(0); // internal attrs
        ds << static_cast<quint32>(0); // external attrs
        ds << static_cast<quint32>(0xFFFFFFFFu); // local offset
        ds.writeRawData("file", 4); // name
        ds << static_cast<qint8>('0' + i); // name (contd.)
        ds.writeRawData(extra.buffer(), uint(extra.size()));
    }
    qint64 centralEnd = fakeLargeFile.pos();
    // zip64 end
    ds << static_cast<quint32>(0x06064b50); // zip64 end magic
    ds << static_cast<quint64>(44); // size of the zip64 end
    ds << static_cast<quint16>(45); // version made by
    ds << static_cast<quint16>(45); // version needed
    ds << static_cast<quint32>(0); // disk number
    ds << static_cast<quint32>(0); // central dir disk number
    ds << static_cast<quint64>(2); // number of entries on disk
    ds << static_cast<quint64>(2); // total number of entries
    ds << static_cast<quint64>(centralEnd - centralStart); // size
    ds << static_cast<quint64>(centralStart); // offset
    // zip64 locator
    ds << static_cast<quint32>(0x07064b50); // zip64 locator magic
    ds << static_cast<quint32>(0); // disk number
    ds << static_cast<quint64>(centralEnd); // offset
    ds << static_cast<quint32>(1); // number of disks
    // zip32 end
    ds << static_cast<quint32>(0x06054b50); // end magic
    ds << static_cast<quint16>(0); // disk number
    ds << static_cast<quint16>(0); // central dir disk number
    ds << static_cast<quint16>(2); // number of entries
    ds << static_cast<quint32>(0xFFFFFFFFu); // central dir size
    ds << static_cast<quint32>(0xFFFFFFFFu); // central dir offset
    ds << static_cast<quint16>(0); // comment length
    fakeLargeFile.close();
    QuaZip fakeLargeZip("tmp/large.zip");
    QVERIFY(fakeLargeZip.open(QuaZip::mdUnzip));
    auto fileInfoList = fakeLargeZip.getFileInfoList();
    QCOMPARE(fileInfoList.size(), numFiles);
    QCOMPARE(fileInfoList[0].uncompressedSize, quint64(0));
    fakeLargeZip.close();
    curDir.remove("tmp/large.zip");
}
