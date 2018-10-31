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

#include "testquazip.h"

#include "qztest.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextCodec>

#include "quazip/quazip.h"
#include "quazip/JlCompress.h"

void TestQuaZip::fileList_data()
{
    QTest::addColumn<QStringList>("fileNames");

    QTest::newRow("simple") << (QStringList()
        << QLatin1String("test0.txt") << QLatin1String("testdir1/test1.txt")
        << QLatin1String("testdir2/test2.txt")
        << QLatin1String("testdir2/subdir/test2sub.txt"));
    QTest::newRow("russian") << (QStringList()
        << QString::fromLatin1("test0.txt")
        << QString::fromLatin1("test1/test1.txt")
        << QString::fromUtf8("testdir2/русский.txt")
        << QString::fromLatin1("testdir2/subdir/test2sub.txt"));
    QTest::newRow("japanese")
        << (QStringList() << QString::fromUtf8("わたしはジップファイル.txt"));
    QTest::newRow("hebrew")
        << (QStringList() << QString::fromUtf8("פתח תקווה.txt"));
}

void TestQuaZip::fileList()
{
    QFETCH(QStringList, fileNames);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    QVERIFY(createTestFiles(fileNames, -1, filesPath));
    QVERIFY(createTestArchive(zipPath, fileNames, nullptr, filesPath));

    QuaZip testZip(zipPath);
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    QVERIFY(testZip.goToFirstFile());
    QString firstFile = testZip.currentFilePath();
    QStringList fileList = testZip.filePathList();
    qSort(fileList);
    QCOMPARE(fileList, fileNames);
    QHash<QString, QFileInfo> srcInfo;
    for (const QString &fileName : fileNames) {
        srcInfo[fileName].setFile(dir.filePath(fileName));
    }

    QList<QuaZipFileInfo> destList = testZip.fileInfoList();
    QCOMPARE(destList.size(), srcInfo.size());
    for (int i = 0; i < destList.size(); i++) {
        QCOMPARE(destList[i].uncompressedSize(),
            srcInfo[destList[i].filePath()].size());
    }
    // test that we didn't mess up the current file
    QCOMPARE(testZip.currentFilePath(), firstFile);
    testZip.close();
}

void TestQuaZip::addFiles_data()
{
    QTest::addColumn<QStringList>("fileNames");
    QTest::addColumn<QStringList>("fileNamesToAdd");
    QTest::newRow("simple")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/test2sub.txt")
        << (QStringList() << "testAdd.txt"
                          << QString::fromUtf8(
                                 "わたしはジップファイル.japanese"))
        << QString::fromUtf8("פתח תקווה.hebrew");
}

void TestQuaZip::addFiles()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QStringList, fileNamesToAdd);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    QVERIFY(createTestFiles(fileNames, -1, filesPath));
    QVERIFY(createTestArchive(zipPath, fileNames, nullptr, filesPath));
    QVERIFY2(createTestFiles(fileNamesToAdd, -1, filesPath),
        "Can't create test files to add");

    QuaZip testZip(zipPath);
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    QString expectedGlobalComment = testZip.globalComment();
    QCOMPARE(testZip.entryCount(), fileNames.length());
    testZip.close();
    QVERIFY(testZip.open(QuaZip::mdAdd));
    for (const QString &fileName : fileNamesToAdd) {
        auto filePath = dir.filePath(fileName);
        QuaZipFile testFile(&testZip);
        testFile.setFilePath(filePath);
        QVERIFY(testFile.open(QIODevice::WriteOnly));
        QFile inFile(filePath);
        QVERIFY(inFile.open(QIODevice::ReadOnly));
        QCOMPARE(testFile.write(inFile.readAll()), inFile.size());
    }
    testZip.close();
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    QStringList allNames = fileNames + fileNamesToAdd;
    QCOMPARE(testZip.entryCount(), allNames.size());
    QCOMPARE(testZip.filePathList(), allNames);
    QCOMPARE(testZip.globalComment(), expectedGlobalComment);
    testZip.close();
}

void TestQuaZip::filePathCodec_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QByteArray, codecName);
    QADD_COLUMN(QByteArray, incompatibleCodecName);

    QTest::newRow("japanese")
        << (QStringList() << QString::fromUtf8("тест.txt")
                          << QString::fromUtf8("わたしはジップファイル.txt"))
        << QByteArray("Shift_JIS") << QByteArray("IBM866");

    QTest::newRow("hebrew")
        << (QStringList() << QString::fromUtf8("פתח תקווה.txt"))
        << QByteArray("UTF-8") << QByteArray("Shift_JIS");
}

void TestQuaZip::filePathCodec()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QByteArray, codecName);
    QFETCH(QByteArray, incompatibleCodecName);

    qSort(fileNames);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    SaveDefaultZipOptions saveCompatibility;
    QuaZip::setDefaultCompatibilityFlags(QuaZip::CustomCompatibility);
    QuaZip::setDefaultFilePathCodec(codecName);

    QVERIFY(createTestFiles(fileNames, -1, filesPath));
    QVERIFY(createTestArchive(zipPath, fileNames, nullptr, filesPath));

    QuaZip::setDefaultFilePathCodec(incompatibleCodecName);

    QuaZip testZip(zipPath);
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    QStringList fileList = testZip.filePathList();
    qSort(fileList);
    QVERIFY(fileList != fileNames);
    testZip.close();
    testZip.setFilePathCodec(codecName);
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    fileList = testZip.filePathList();
    qSort(fileList);
    QCOMPARE(fileList, fileNames);
    testZip.close();
}

void TestQuaZip::commentCodec_data()
{
    QADD_COLUMN(QString, comment);
    QADD_COLUMN(QByteArray, codecName);
    QADD_COLUMN(QByteArray, incompatibleCodecName);

    QTest::newRow("russian")
        << QString::fromUtf8("Большой Русский Комментарий")
        << QByteArray("windows-1251") << QByteArray("IBM850");

    QTest::newRow("multilingual")
        << (QStringList() << QString::fromUtf8(
                "פתח תקווה わたしはジップファイル"))
        << QByteArray("UTF-8") << QByteArray("Shift_JIS");
}

void TestQuaZip::commentCodec()
{
    QFETCH(QString, comment);
    QFETCH(QByteArray, codecName);
    QFETCH(QByteArray, incompatibleCodecName);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    QString testFileName("dummy");

    QuaZip zip(zipPath);
    QVERIFY(zip.open(QuaZip::mdCreate));
    zip.setCompatibilityFlags(QuaZip::CustomCompatibility);
    zip.setFilePathCodec(incompatibleCodecName);
    zip.setCommentCodec(codecName);
    zip.setGlobalComment(comment);
    {
        QuaZipFile zipFile(&zip, testFileName);
        zipFile.setComment(comment);
        QVERIFY(zipFile.open(QIODevice::WriteOnly));
    }
    zip.close();

    QVERIFY(zip.open(QuaZip::mdUnzip));
    QCOMPARE(zip.globalComment(), comment);
    {
        QuaZipFile zipFile(&zip, testFileName);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        QCOMPARE(zipFile.comment(), comment);
    }
    zip.close();

    zip.setCommentCodec(incompatibleCodecName);
    QVERIFY(zip.open(QuaZip::mdUnzip));
    QVERIFY(zip.globalComment() != comment);
    {
        QuaZipFile zipFile(&zip, testFileName);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        QVERIFY(zipFile.comment() != comment);
    }
    zip.close();
}

void TestQuaZip::dataDescriptorWritingEnabled()
{
    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    QString testFileName("vegetation_info.xml");

    quint32 magic;
    quint16 versionNeeded;
    auto contents = QByteArrayLiteral("<vegetation_info version=\"4096\" />\n");
    {
        QuaZip testZip(zipPath);
        testZip.setCompatibilityFlags(QuaZip::DosCompatible);
        testZip.setDataDescriptorWritingEnabled(false);
        QVERIFY(testZip.open(QuaZip::mdCreate));
        QuaZipFile testZipFile(&testZip, testFileName);
        QVERIFY(testZipFile.open(QIODevice::WriteOnly));
        QCOMPARE(testZipFile.write(contents), qint64(contents.length()));
        testZipFile.close();
        testZip.close();
        QuaZipFile readZipFile(zipPath, testFileName);
        QVERIFY(readZipFile.open(QIODevice::ReadOnly));
        // Test that file is not compressed.
        QCOMPARE(readZipFile.compressedSize(), qint64(contents.size()));
        readZipFile.close();
        QCOMPARE(QFileInfo(zipPath).size(), qint64(171));
        QFile zipFile(zipPath);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        QDataStream zipData(&zipFile);
        zipData.setByteOrder(QDataStream::LittleEndian);
        zipData >> magic;
        zipData >> versionNeeded;
        QCOMPARE(magic, quint32(0x04034b50));
        QCOMPARE(versionNeeded, quint16(10));
        zipFile.close();

        QVERIFY2(QFile::remove(zipPath), "Can't remove zip file");
    }

    {
        QuaZip testZip(zipPath);
        testZip.setCompatibilityFlags(QuaZip::DosCompatible);
        QVERIFY(testZip.isDataDescriptorWritingEnabled());
        QVERIFY(testZip.open(QuaZip::mdCreate));
        QuaZipFile testZipFile(&testZip, testFileName);
        QVERIFY(testZipFile.open(QIODevice::WriteOnly));
        QCOMPARE(testZipFile.write(contents), qint64(contents.length()));
        testZipFile.close();
        testZip.close();
        QCOMPARE(QFileInfo(zipPath).size(),
            qint64(171 + 16)); // 16 bytes = data descriptor
    }

    QFile zipFile(zipPath);
    QVERIFY(zipFile.open(QIODevice::ReadOnly));
    QDataStream zipData(&zipFile);
    zipData.setByteOrder(QDataStream::LittleEndian);
    zipData >> magic;
    zipData >> versionNeeded;
    QCOMPARE(magic, quint32(0x04034b50));
    QCOMPARE(versionNeeded, quint16(20));
    zipFile.close();
}

void TestQuaZip::testQIODeviceAPI()
{
    QString zipName = "qiodevice_api.zip";
    QStringList fileNames;
    fileNames << "test.txt";
    QDir curDir;
    if (curDir.exists(zipName)) {
        if (!curDir.remove(zipName))
            QFAIL("Can't remove zip file");
    }
    if (!createTestFiles(fileNames)) {
        QFAIL("Can't create test file");
    }
    if (!createTestArchive(zipName, fileNames)) {
        QFAIL("Can't create test archive");
    }
    QBuffer buffer;
    if (!createTestArchive(&buffer, fileNames, NULL)) {
        QFAIL("Can't create test archive");
    }
    QFile diskFile(zipName);
    QVERIFY(diskFile.open(QIODevice::ReadOnly));
    QByteArray bufferArray = buffer.buffer();
    QByteArray fileArray = diskFile.readAll();
    diskFile.close();
    QCOMPARE(bufferArray.size(), fileArray.size());
    QCOMPARE(bufferArray, fileArray);
    curDir.remove(zipName);
}

void TestQuaZip::zipFilePath()
{
    QuaZip zip;
    zip.setZipFilePath("testsetZipFilePath.zip");
    zip.open(QuaZip::mdCreate);
    zip.close();
    QVERIFY(QFileInfo(zip.zipFilePath()).exists());
    QDir().remove(zip.zipFilePath());
}

void TestQuaZip::ioDevice()
{
    QuaZip zip;
    QFile file("testSetIoDevice.zip");
    zip.setIODevice(&file);
    QCOMPARE(zip.ioDevice(), &file);
    zip.open(QuaZip::mdCreate);
    QVERIFY(file.isOpen());
    zip.close();
    QVERIFY(!file.isOpen());
    QVERIFY(file.exists());
    QDir().remove(file.fileName());
}

void TestQuaZip::autoClose()
{
    {
        QBuffer buf;
        QuaZip zip(&buf);
        QVERIFY(zip.isAutoClose());
        QVERIFY(zip.open(QuaZip::mdCreate));
        QVERIFY(buf.isOpen());
        zip.close();
        QVERIFY(!buf.isOpen());
        QVERIFY(zip.open(QuaZip::mdCreate));
    }
    {
        QBuffer buf;
        QuaZip zip(&buf);
        QVERIFY(zip.isAutoClose());
        zip.setAutoClose(false);
        QVERIFY(!zip.isAutoClose());
        QVERIFY(zip.open(QuaZip::mdCreate));
        QVERIFY(buf.isOpen());
        zip.close();
        QVERIFY(buf.isOpen());
        QVERIFY(zip.open(QuaZip::mdCreate));
    }
}

#ifdef QUAZIP_TEST_QSAVEFILE
void TestQuaZip::saveFileBug()
{
    QDir curDir;
    QString zipName = "testSaveFile.zip";
    if (curDir.exists(zipName)) {
        if (!curDir.remove(zipName)) {
            QFAIL("Can't remove testSaveFile.zip");
        }
    }
    QuaZip zip;
    QSaveFile saveFile(zipName);
    zip.setIODevice(&saveFile);
    QCOMPARE(zip.ioDevice(), &saveFile);
    zip.open(QuaZip::mdCreate);
    zip.close();
    QVERIFY(QFileInfo(saveFile.fileName()).exists());
    curDir.remove(saveFile.fileName());
}
#endif

void TestQuaZip::testSequential()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress(QHostAddress::LocalHost)));
    quint16 port = server.serverPort();
    QTcpSocket socket;
    socket.connectToHost(QHostAddress(QHostAddress::LocalHost), port);
    QVERIFY(socket.waitForConnected());
    QVERIFY(server.waitForNewConnection(30000));
    QTcpSocket *client = server.nextPendingConnection();
    QuaZip zip(&socket);
    zip.setAutoClose(false);
    QVERIFY(zip.open(QuaZip::mdCreate));
    QVERIFY(socket.isOpen());
    QuaZipFile zipFile(&zip);
    QuaZipNewInfo info("test.txt");
    QVERIFY(zipFile.open(QIODevice::WriteOnly, info, NULL, 0, 0));
    QCOMPARE(zipFile.write("test"), static_cast<qint64>(4));
    zipFile.close();
    zip.close();
    QVERIFY(socket.isOpen());
    socket.disconnectFromHost();
    QVERIFY(socket.waitForDisconnected());
    QVERIFY(client->waitForReadyRead());
    QByteArray received = client->readAll();
#ifdef QUAZIP_QZTEST_QUAZIP_DEBUG_SOCKET
    QFile debug("testSequential.zip");
    debug.open(QIODevice::WriteOnly);
    debug.write(received);
    debug.close();
#endif
    client->close();
    QBuffer buffer(&received);
    QuaZip receivedZip(&buffer);
    QVERIFY(receivedZip.open(QuaZip::mdUnzip));
    QVERIFY(receivedZip.goToFirstFile());
    QuaZipFileInfo receivedInfo;
    QVERIFY(receivedZip.getCurrentFileInfo(&receivedInfo));
    QCOMPARE(receivedInfo.name, QString::fromLatin1("test.txt"));
    QCOMPARE(receivedInfo.uncompressedSize, static_cast<quint64>(4));
    QuaZipFile receivedFile(&receivedZip);
    QVERIFY(receivedFile.open(QIODevice::ReadOnly));
    QByteArray receivedText = receivedFile.readAll();
    QCOMPARE(receivedText, QByteArray("test"));
    receivedFile.close();
    receivedZip.close();
}
