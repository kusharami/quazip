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

#include "testjlcompress.h"

#include "qztest.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include "quazip/JlCompress.h"
#include "quazip/quazipfileinfo.h"

void TestJlCompress::compressFile_data()
{
    QADD_COLUMN(QString, fileName);
    QADD_COLUMN(bool, useDevice);

    QTest::newRow("filepath zip") << "test0.txt" << false;
    QTest::newRow("device zip") << "test0.txt" << true;
}

void TestJlCompress::compressFile()
{
    QFETCH(QString, fileName);
    QFETCH(bool, useDevice);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    QVERIFY2(createTestFiles(QStringList() << fileName, -1, filesPath),
        "Can't create test file");

    QStringList fileList;
    if (useDevice) {
        QVERIFY(JlCompress::compressFile(zipPath, dir.filePath(fileName)));
        fileList = JlCompress::getFileList(zipPath);
    } else {
        QFile zipFile(zipPath);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        fileList = JlCompress::getFileList(&zipFile);
    }
    QCOMPARE(fileList.count(), 1);
    QCOMPARE(fileList.at(0), fileName);
}

void TestJlCompress::compressFiles_data()
{
    QADD_COLUMN(QStringList, fileNames);

    QTest::newRow("simple") << (QStringList() << "test0.txt"
                                              << "test00.txt");
    QTest::newRow("different subdirs")
        << (QStringList() << "subdir1/test1.txt"
                          << "subdir2/test2.txt");
}

void TestJlCompress::compressFiles()
{
    QFETCH(QStringList, fileNames);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    QVERIFY2(
        createTestFiles(fileNames, -1, filesPath), "Can't create test files");

    QStringList realNamesList, shortNamesList;
    for (const auto &fileName : fileNames) {
        QString realName = dir.filePath(fileName);
        realNamesList += realName;
        shortNamesList += QFileInfo(realName).fileName();
    }
    QVERIFY(JlCompress::compressFiles(zipPath, realNamesList));
    // get the file list and check it
    QStringList fileList = JlCompress::getFileList(zipPath);
    qSort(fileList);
    qSort(shortNamesList);
    QCOMPARE(fileList, shortNamesList);
}

void TestJlCompress::compressDir_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QStringList, expected);

    QTest::newRow("simple")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/test2sub.txt")
        << (QStringList() << "test0.txt"
                          << "testdir1/"
                          << "testdir1/test1.txt"
                          << "testdir2/"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/"
                          << "testdir2/subdir/test2sub.txt");
    QTest::newRow("empty dirs") << (QStringList() << "testdir1/"
                                                  << "testdir2/testdir3/")
                                << (QStringList() << "testdir1/"
                                                  << "testdir2/"
                                                  << "testdir2/testdir3/");
    QTest::newRow("hidden files") << (QStringList() << ".test0.txt"
                                                    << "test1.txt")
                                  << (QStringList() << ".test0.txt"
                                                    << "test1.txt");
}

void TestJlCompress::compressDir()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QStringList, expected);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    QVERIFY2(
        createTestFiles(fileNames, -1, filesPath), "Can't create test files");

    QuaZipFileInfo attrInfo;
    attrInfo.setAttributes(QuaZipFileInfo::Hidden);

    for (const auto &fileName : fileNames) {
        if (fileName.startsWith('.'))
            QuaZipFileInfo::applyAttributesTo(
                dir.filePath(fileName), QuaZipFileInfo::Hidden);
    }

    QVERIFY(JlCompress::compressDir(zipPath, filesPath, true, QDir::Hidden));
    // get the file list and check it
    QStringList fileList = JlCompress::getFileList(zipPath);
    qSort(fileList);
    qSort(expected);
    QCOMPARE(fileList, expected);
}

void TestJlCompress::extractFile_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QString, fileToExtract);
    QADD_COLUMN(QString, destName);
    QADD_COLUMN(QByteArray, encoding);
    QADD_COLUMN(bool, useDevice);

    QTest::newRow("simple") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << "testdir2/test2.txt"
                            << "test2.txt" << QByteArray() << false;
    QTest::newRow("russian")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << QString::fromUtf8("testdir2/тест2.txt")
                          << "testdir2/subdir/test2sub.txt")
        << QString::fromUtf8("testdir2/тест2.txt")
        << QString::fromUtf8("тест2.txt") << QByteArray("IBM866") << true;
    QTest::newRow("extract dir")
        << (QStringList() << "testdir1/") << "testdir1/"
        << "testdir1/" << QByteArray() << false;
}

void TestJlCompress::extractFile()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QString, fileToExtract);
    QFETCH(QString, destName);
    QFETCH(QByteArray, encoding);
    QFETCH(bool, useDevice);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir curDir(tempDir.path());
    QDir dir(filesPath);
    QDir targetDir(curDir.filePath("jlext/jlfile"));
    QVERIFY2(curDir.mkpath(targetDir.path()), "Couldn't make extract path");

    QVERIFY2(createTestFiles(fileNames, -1, filesPath),
        "Couldn't create test files");

    QFileInfo srcInfo(dir.filePath(fileToExtract));
    QuaZipFileInfo::applyAttributesTo(
        srcInfo.filePath(), QuaZipFileInfo::ReadOnly);

    QVERIFY2(!createTestArchive(zipPath, fileNames,
                 QTextCodec::codecForName(encoding), filesPath),
        "Can't create test archive");

    SaveDefaultZipOptions saveZipOptions;
    QuaZip::setDefaultFilePathCodec(encoding);

    QFileInfo destInfo(targetDir.filePath(destName));
    if (useDevice) {
        QFile zipFile(zipPath);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        QVERIFY(!JlCompress::extractFile(
            &zipFile, fileToExtract, destInfo.filePath())
                     .isEmpty());
    } else {
        QVERIFY(!JlCompress::extractFile(
            zipPath, fileToExtract, destInfo.filePath())
                     .isEmpty());
    }
    QCOMPARE(destInfo.size(), srcInfo.size());
    QCOMPARE(destInfo.permissions(), srcInfo.permissions());
    QVERIFY(QFile::remove(destInfo.filePath()));

    if (!fileToExtract.endsWith('/')) {
        QVERIFY(curDir.mkdir(destInfo.filePath()));
        if (useDevice) {
            QFile zipFile(zipPath);
            QVERIFY(zipFile.open(QIODevice::ReadOnly));
            QVERIFY2(JlCompress::extractFile(
                         &zipFile, fileToExtract, destInfo.filePath())
                         .isEmpty(),
                "Directory existed with the same name of the file "
                "should not be replaced.");
        } else {
            QVERIFY2(JlCompress::extractFile(
                         zipPath, fileToExtract, destInfo.filePath())
                         .isEmpty(),
                "Directory existed with the same name of the file "
                "should not be replaced.");
        }
    }
}

void TestJlCompress::extractFiles_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QStringList, filesToExtract);
    QADD_COLUMN(bool, useDevice);

    QTest::newRow("filepath zip")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/test2sub.txt")
        << (QStringList() << "testdir2/test2.txt"
                          << "testdir1/test1.txt")
        << false;
    QTest::newRow("device zip")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/test2sub.txt")
        << (QStringList() << "testdir2/test2.txt"
                          << "testdir1/test1.txt")
        << true;
}

void TestJlCompress::extractFiles()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QStringList, filesToExtract);
    QFETCH(bool, useDevice);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir curDir(tempDir.path());
    QDir dir(filesPath);
    QDir targetDir(curDir.filePath("jlext/jlfiles"));
    QVERIFY2(curDir.mkpath(targetDir.path()), "Couldn't make extract path");

    QVERIFY2(createTestFiles(fileNames, -1, filesPath),
        "Couldn't create test files");

    for (const QString &fileName : filesToExtract) {
        QFileInfo srcInfo(dir.filePath(fileName));
        QuaZipFileInfo::applyAttributesTo(
            srcInfo.filePath(), QuaZipFileInfo::ReadOnly);
    }

    QVERIFY2(JlCompress::compressDir(zipPath, filesPath),
        "Couldn't create test archive");

    if (useDevice) {
        QFile zipFile(zipPath);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        QVERIFY(!JlCompress::extractFiles(
            &zipFile, filesToExtract, targetDir.path())
                     .isEmpty());
    } else {
        QVERIFY(
            !JlCompress::extractFiles(zipPath, filesToExtract, targetDir.path())
                 .isEmpty());
    }
    for (const QString &fileName : filesToExtract) {
        QFileInfo dstInfo(targetDir.filePath(fileName));
        QFileInfo srcInfo(dir.filePath(fileName));
        QCOMPARE(dstInfo.size(), srcInfo.size());
        QCOMPARE(dstInfo.permissions(), srcInfo.permissions());
    }
}

void TestJlCompress::extractDir_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QStringList, expectedExtracted);
    QADD_COLUMN(bool, useDevice);

    QTest::newRow("simple") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << false;
    QTest::newRow("separate dir") << (QStringList() << "laj/"
                                                    << "laj/lajfile.txt")
                                  << (QStringList() << "laj/"
                                                    << "laj/lajfile.txt")
                                  << true;
    QTest::newRow("FilePath Zip Slip")
        << (QStringList() << "test0.txt"
                          << "../zipslip.txt")
        << (QStringList() << "test0.txt") << false;
    QTest::newRow("Device Zip Slip") << (QStringList() << "test0.txt"
                                                       << "../zipslip.txt")
                                     << (QStringList() << "test0.txt") << true;
}

void TestJlCompress::extractDir()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QStringList, expectedExtracted);
    QFETCH(bool, useDevice);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir curDir(tempDir.path());
    QDir dir(filesPath);
    QDir targetDir(curDir.filePath("jlext/jldir"));
    QVERIFY2(curDir.mkpath(targetDir.path()), "Couldn't make extract path");

    QVERIFY2(createTestFiles(fileNames, -1, filesPath),
        "Couldn't create test files");

    QVERIFY2(createTestArchive(zipPath, fileNames, filesPath),
        "Couldn't create test archive");

    QStringList extracted;
    if (useDevice) {
        QFile zipFile(zipPath);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        extracted = JlCompress::extractDir(&zipFile, targetDir.path());
    } else {
        extracted = JlCompress::extractDir(zipPath, targetDir.path());
    }
    QCOMPARE(extracted.count(), expectedExtracted.count());
    for (const QString &fileName : expectedExtracted) {
        QString fullName = targetDir.filePath(fileName);
        QFileInfo dstInfo(fullName);
        QFileInfo srcInfo(dir.filePath(fileName));
        QCOMPARE(dstInfo.size(), srcInfo.size());
        QCOMPARE(dstInfo.permissions(), srcInfo.permissions());

        QString absolutePath = targetDir.absoluteFilePath(fileName);
        if (dstInfo.isDir() && !absolutePath.endsWith('/'))
            absolutePath += '/';
        QVERIFY(extracted.contains(absolutePath));
    }
}
