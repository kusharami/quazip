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
#include "quazip/quaziptextcodec.h"
#include "quazip/JlCompress.h"
#include "quazip/quacrc32.h"

static QString nameForDos(const QString &name)
{
    auto codec = QuaZipTextCodec::codecForCodepage(0);
    if (codec->canEncode(name))
        return name;

    return QStringLiteral("%1").arg(
        zChecksum<QuaCrc32>(name.constData(), name.length() * sizeof(QChar)), 8,
        16, QChar('0'));
}

void TestQuaZip::create_data()
{
    QADD_COLUMN(QuaZip::Compatibility, compatibility);
    QADD_COLUMN(QByteArray, appendTo);
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QStringList, expectedFileNames);
    QADD_COLUMN(QString, comment);
    QADD_COLUMN(QString, expectedComment);

    QString dirRus = QString::fromUtf8("папка");
    QString testDirFmt("test/dir/%1/");
    QString rusExtFmt("%1/%2");
    QString japExtFile = QString::fromUtf8("file.わたし");

    QStringList testFiles;
    testFiles << "testZipName.txt";
    testFiles << testDirFmt.arg(dirRus);
    testFiles << rusExtFmt.arg(dirRus, japExtFile);

    QStringList dosFiles;
    dosFiles << "testZi~1.txt";
    dosFiles << testDirFmt.arg(nameForDos(dirRus));
    dosFiles << rusExtFmt.arg(nameForDos(dirRus), nameForDos(japExtFile));

    QString latinComment("lorem ipsum");
    auto unicodeComment =
        QString::fromUtf8("SUPER COMMENT わたしはジップファイル פתח תקווה");

    QTest::newRow("default")
        << QuaZip::Compatibility(QuaZip::DefaultCompatibility) << QByteArray()
        << testFiles << testFiles << unicodeComment << unicodeComment;

    QTest::newRow("dos only")
        << QuaZip::Compatibility(QuaZip::DosCompatible) << QByteArray()
        << testFiles << dosFiles << unicodeComment
        << QString::fromLatin1(
               QuaZipTextCodec::codecForCodepage(0)->fromUnicode(
                   unicodeComment));

    QTest::newRow("dos+append")
        << QuaZip::Compatibility(QuaZip::DosCompatible)
        << QByteArray(16, Qt::Uninitialized) << testFiles << dosFiles
        << latinComment << latinComment;

    QTest::newRow("unix") << QuaZip::Compatibility(QuaZip::UnixCompatible)
                          << QByteArray() << testFiles << testFiles
                          << unicodeComment << unicodeComment;

    QTest::newRow("windows+append")
        << QuaZip::Compatibility(QuaZip::WindowsCompatible)
        << QByteArray(33, Qt::Uninitialized) << testFiles << testFiles
        << unicodeComment << unicodeComment;

    QTest::newRow("custom compatibility")
        << QuaZip::Compatibility(QuaZip::CustomCompatibility) << QByteArray()
        << testFiles << testFiles << unicodeComment << unicodeComment;

    QTest::newRow("full compatibility+append")
        << QuaZip::Compatibility(QuaZip::FullCompatibility)
        << QByteArray(16, Qt::Uninitialized) << testFiles << testFiles
        << unicodeComment << unicodeComment;
}

void TestQuaZip::create()
{
    QFETCH(QuaZip::Compatibility, compatibility);
    QFETCH(QByteArray, appendTo);
    QFETCH(QStringList, fileNames);
    QFETCH(QStringList, expectedFileNames);
    QFETCH(QString, comment);
    QFETCH(QString, expectedComment);

    bool append = !appendTo.isEmpty();
    auto openMode = append ? QuaZip::mdAppend : QuaZip::mdCreate;

    QBuffer buffer;
    if (append) {
        QVERIFY(buffer.open(QBuffer::WriteOnly));
        QCOMPARE(buffer.write(appendTo), qint64(appendTo.length()));
        buffer.close();
    }
    {
        QuaZip zip(&buffer);
        QCOMPARE(zip.compatibility(), QuaZip::DefaultCompatibility);
        zip.setCompatibility(compatibility);
        QCOMPARE(zip.compatibility(), compatibility);
        zip.setFilePathCodec("IBM850");
        QCOMPARE(zip.filePathCodec(),
            QTextCodec::codecForMib(QuaZipTextCodec::IANA_IBM850));
        zip.setGlobalComment(comment);
        QCOMPARE(zip.globalComment(), comment);
        QVERIFY(!zip.isOpen());
        QVERIFY(zip.open(openMode));
        QCOMPARE(zip.openMode(), openMode);
        QVERIFY(zip.isOpen());
        for (const auto &fileName : fileNames) {
            QuaZipFile zipFile(&zip, fileName);
            QVERIFY(zipFile.open(QIODevice::Truncate));
            zipFile.close();
        }
        zip.close();
        QCOMPARE(zip.zipError(), 0);
    }

    QVERIFY(buffer.open(QBuffer::ReadOnly));
    QVERIFY(buffer.seek(appendTo.length()));
    {
        QuaZip zip(&buffer);
        zip.setCompatibility(compatibility);
        zip.setFilePathCodec("IBM850");
        QVERIFY(zip.open(QuaZip::mdUnzip));
        QVERIFY(zip.isOpen());
        QCOMPARE(zip.openMode(), QuaZip::mdUnzip);
        QCOMPARE(zip.globalComment(), expectedComment);
        QCOMPARE(zip.entryCount(), expectedFileNames.count());
        QCOMPARE(zip.filePathList(), expectedFileNames);
        zip.close();
        QCOMPARE(zip.zipError(), 0);
    }
}

void TestQuaZip::fileList_data()
{
    QADD_COLUMN(QStringList, fileNames);

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
    QCOMPARE(testZip.zipError(), 0);
}

void TestQuaZip::addFiles_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QStringList, fileNamesToAdd);

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
    QVERIFY(!testZip.isOpen());
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    QVERIFY(testZip.isOpen());
    QCOMPARE(testZip.openMode(), QuaZip::mdUnzip);
    QString expectedGlobalComment = testZip.globalComment();
    QCOMPARE(testZip.entryCount(), fileNames.length());
    testZip.close();
    QVERIFY(testZip.open(QuaZip::mdAdd));
    QVERIFY(testZip.isOpen());
    QCOMPARE(testZip.openMode(), QuaZip::mdAdd);
    for (const QString &fileName : fileNamesToAdd) {
        auto filePath = dir.filePath(fileName);
        QuaZipFile testFile(&testZip, filePath);
        QVERIFY(testFile.open(QIODevice::WriteOnly));
        QFile inFile(filePath);
        QVERIFY(inFile.open(QIODevice::ReadOnly));
        QCOMPARE(testFile.write(inFile.readAll()), inFile.size());
    }
    testZip.close();
    QCOMPARE(testZip.zipError(), 0);
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    QStringList allNames = fileNames + fileNamesToAdd;
    QCOMPARE(testZip.entryCount(), allNames.size());
    QCOMPARE(testZip.filePathList(), allNames);
    QCOMPARE(testZip.globalComment(), expectedGlobalComment);
    testZip.close();
    QCOMPARE(testZip.zipError(), 0);
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

    SaveDefaultZipOptions saveZipOptions;
    QuaZip::setDefaultCompatibility(QuaZip::CustomCompatibility);
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
    QCOMPARE(testZip.zipError(), 0);
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
    zip.setCompatibility(QuaZip::CustomCompatibility);
    zip.setFilePathCodec(incompatibleCodecName);
    zip.setCommentCodec(codecName);
    zip.setGlobalComment(comment);
    {
        QuaZipFile zipFile(&zip, testFileName);
        zipFile.setComment(comment);
        QVERIFY(zipFile.open(QIODevice::WriteOnly));
    }
    zip.close();
    QCOMPARE(zip.zipError(), 0);

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
    QCOMPARE(zip.zipError(), 0);
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
        testZip.setCompatibility(QuaZip::DosCompatible);
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
        testZip.setCompatibility(QuaZip::DosCompatible);
        QVERIFY(testZip.isDataDescriptorWritingEnabled());
        QVERIFY(testZip.open(QuaZip::mdCreate));
        QuaZipFile testZipFile(&testZip, testFileName);
        QVERIFY(testZipFile.open(QIODevice::WriteOnly));
        QCOMPARE(testZipFile.write(contents), qint64(contents.length()));
        testZipFile.close();
        testZip.close();
        QCOMPARE(QFileInfo(zipPath).size(),
            qint64(171 + 16)); // 16 bytes = data descriptor
        QCOMPARE(testZip.zipError(), 0);
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
    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);

    QStringList fileNames;
    fileNames << "test.txt";

    QVERIFY(createTestFiles(fileNames, -1, filesPath));
    QVERIFY(createTestArchive(zipPath, fileNames, nullptr, filesPath));

    QBuffer buffer;
    QVERIFY2(createTestArchive(&buffer, fileNames, NULL, filesPath),
        "Can't create test archive");

    QFile diskFile(zipPath);
    QVERIFY(diskFile.open(QIODevice::ReadOnly));
    QByteArray bufferArray = buffer.buffer();
    QByteArray fileArray = diskFile.readAll();
    diskFile.close();
    QCOMPARE(bufferArray, fileArray);
}

void TestQuaZip::setZipFilePath()
{
    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);

    QuaZip zip;
    zip.setZipFilePath(zipPath);
    QCOMPARE(zip.zipFilePath(), zipPath);
    zip.open(QuaZip::mdCreate);
    QVERIFY(zip.ioDevice());
    QVERIFY(zip.ioDevice()->isWritable());
    zip.close();
    QCOMPARE(zip.zipError(), 0);
    QVERIFY(!zip.ioDevice());
    QVERIFY(QFileInfo::exists(zip.zipFilePath()));
}

void TestQuaZip::setIODevice()
{
    QuaZip zip;
    QBuffer buffer;
    zip.setIODevice(&buffer);
    QVERIFY(zip.zipFilePath().isEmpty());
    QCOMPARE(zip.ioDevice(), &buffer);
    zip.open(QuaZip::mdCreate);
    QVERIFY(buffer.isOpen());
    zip.close();
    QCOMPARE(zip.zipError(), 0);
    QVERIFY(!buffer.isOpen());
    QVERIFY(!buffer.data().isEmpty());
}

void TestQuaZip::autoClose_data()
{
    QADD_COLUMN(bool, autoCloseEnabled);

    QTest::newRow("yes") << true;
    QTest::newRow("no") << false;
}

void TestQuaZip::autoClose()
{
    QFETCH(bool, autoCloseEnabled);

    QBuffer buf;
    {
        QuaZip zip(&buf);
        QVERIFY(zip.isAutoClose());
        zip.setAutoClose(autoCloseEnabled);
        QCOMPARE(zip.isAutoClose(), autoCloseEnabled);
        QVERIFY(zip.open(QuaZip::mdCreate));
        zip.close();
        QCOMPARE(zip.zipError(), 0);
    }
    QCOMPARE(!buf.isOpen(), autoCloseEnabled);
    buf.close();
    {
        QVERIFY(buf.open(QIODevice::ReadOnly));
        QuaZip zip(&buf);
        zip.setAutoClose(autoCloseEnabled);
        QVERIFY(zip.open(QuaZip::mdUnzip));
        zip.close();
        QCOMPARE(zip.zipError(), 0);
    }
    QCOMPARE(!buf.isOpen(), autoCloseEnabled);
}

void TestQuaZip::sequentialWrite()
{
    QString testFileName("garbage.bin");
    QByteArray testContent(256, Qt::Uninitialized);

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress(QHostAddress::LocalHost)));
    quint16 port = server.serverPort();
    QTcpSocket socket;
    socket.connectToHost(QHostAddress(QHostAddress::LocalHost), port);
    QVERIFY(socket.waitForConnected());
    QVERIFY(server.waitForNewConnection(10000));
    QTcpSocket *client = server.nextPendingConnection();
    {
        QuaZip zip(&socket);
        zip.setAutoClose(false);
        QVERIFY(zip.open(QuaZip::mdCreate));
        QVERIFY(socket.isOpen());
        QuaZipFile zipFile(&zip, testFileName);
        QVERIFY(zipFile.open(QIODevice::WriteOnly));
        QCOMPARE(zipFile.write(testContent), qint64(testContent.length()));
        zipFile.close();
        QCOMPARE(zipFile.zipError(), 0);
        zip.close();
        QCOMPARE(zip.zipError(), 0);
    }
    QVERIFY(socket.isOpen());
    socket.disconnectFromHost();
    QVERIFY(socket.waitForDisconnected());
    QVERIFY(client->waitForReadyRead());
    QByteArray received = client->readAll();
    client->close();
    QBuffer buffer(&received);
    QuaZip receivedZip(&buffer);
    QVERIFY(receivedZip.open(QuaZip::mdUnzip));
    QVERIFY(receivedZip.goToFirstFile());
    QuaZipFileInfo receivedInfo;
    QVERIFY(receivedZip.getCurrentFileInfo(receivedInfo));
    QCOMPARE(receivedInfo.filePath(), testFileName);
    QCOMPARE(receivedInfo.uncompressedSize(), testContent.length());
    QuaZipFile receivedFile(&receivedZip);
    QVERIFY(receivedFile.open(QIODevice::ReadOnly));
    QByteArray receivedText = receivedFile.readAll();
    QCOMPARE(receivedText, testContent);
    receivedFile.close();
    receivedZip.close();
    QCOMPARE(receivedZip.zipError(), 0);
}
