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
    QADD_COLUMN(QString, targetDir);

    QTest::newRow("target dir") << QString("blabla");
    QTest::newRow("root dir") << QString('.');
}

void TestJlCompress::compressFile()
{
    QFETCH(QString, targetDir);
    QString fileName("test0.txt");

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    QVERIFY2(createTestFiles(QStringList() << fileName, -1, filesPath),
        "Can't create test file");

    QStringList fileList;
    QVERIFY(
        JlCompress::compressFile(zipPath, dir.filePath(fileName), targetDir));
    fileList = JlCompress::getFileList(zipPath);
    QCOMPARE(fileList.count(), 1);
    QFileInfo zippedFileInfo(fileList.at(0));
    QCOMPARE(zippedFileInfo.fileName(), fileName);
    QCOMPARE(zippedFileInfo.path(), targetDir);
    QFile file(zipPath);
    QCOMPARE(JlCompress::getFileList(&file), fileList);
}

void TestJlCompress::compressFiles_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QString, targetDir);

    QTest::newRow("simple") << (QStringList() << "test0.txt"
                                              << "test00.txt")
                            << QString("store");
    QTest::newRow("different subdirs") << (QStringList() << "subdir1/test1.txt"
                                                         << "subdir2/test2.txt")
                                       << QString();
}

void TestJlCompress::compressFiles()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QString, targetDir);

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
        shortNamesList += QDir::cleanPath(
            QDir(targetDir).filePath(QFileInfo(realName).fileName()));
    }
    QVERIFY(JlCompress::compressFiles(zipPath, realNamesList, targetDir));
    // get the file list and check it
    QStringList fileList = JlCompress::getFileList(zipPath);
    {
        QFile file(zipPath);
        QCOMPARE(JlCompress::getFileList(&file), fileList);
    }
    qSort(fileList);
    qSort(shortNamesList);
    QCOMPARE(fileList, shortNamesList);
}

void TestJlCompress::compressDir_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QStringList, expected);
    QADD_COLUMN(QDir::Filters, filters);

    QTest::newRow("simple") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << (QStringList() << "test0.txt"
                                              << "testdir1/"
                                              << "testdir1/test1.txt"
                                              << "testdir2/"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/"
                                              << "testdir2/subdir/test2sub.txt")
                            << QDir::Filters();
    QTest::newRow("empty dirs") << (QStringList() << ".test0.txt"
                                                  << "testdir1/"
                                                  << "testdir2/testdir3/")
                                << (QStringList() << "testdir1/"
                                                  << "testdir2/"
                                                  << "testdir2/testdir3/")
                                << QDir::Filters();
    QTest::newRow("hidden files") << (QStringList() << ".test0.txt"
                                                    << "test1.txt")
                                  << (QStringList() << ".test0.txt"
                                                    << "test1.txt")
                                  << QDir::Filters(QDir::Hidden);
}

void TestJlCompress::compressDir()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QStringList, expected);
    QFETCH(QDir::Filters, filters);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    QVERIFY2(
        createTestFiles(fileNames, -1, filesPath), "Can't create test files");

    for (const auto &fileName : fileNames) {
        if (fileName.startsWith('.')) {
            QuaZipFileInfo::applyAttributesTo(
                dir.filePath(fileName), QuaZipFileInfo::Hidden);
        }
    }

    QVERIFY(JlCompress::compressDir(zipPath, filesPath, true, filters));
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

    SaveDefaultZipOptions saveZipOptions;
    QuaZip::setDefaultFilePathCodec(encoding);
    QuaZip::setDefaultCompatibility(QuaZip::CustomCompatibility);

    auto permissions = srcInfo.permissions();
    QVERIFY(createTestArchive(zipPath, fileNames, nullptr, filesPath));

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
    auto destPermissions = destInfo.permissions();
    QuaZipFileInfo::applyAttributesTo(
        destInfo.filePath(), QuaZipFileInfo::NoAttr);
    QCOMPARE(destInfo.size(), srcInfo.size());
    QCOMPARE(destPermissions, permissions);
    if (destInfo.isDir() && !destInfo.isSymLink())
        QVERIFY(QDir(destInfo.filePath()).removeRecursively());
    else
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

    QVector<QFile::Permissions> permissions;

    for (const QString &fileName : filesToExtract) {
        QFileInfo srcInfo(dir.filePath(fileName));
        QuaZipFileInfo::applyAttributesTo(
            srcInfo.filePath(), QuaZipFileInfo::ReadOnly);
        srcInfo.refresh();
        permissions.append(srcInfo.permissions());
    }

    bool compressDirOk = JlCompress::compressDir(zipPath, filesPath);
    for (const QString &fileName : filesToExtract) {
        QFileInfo srcInfo(dir.filePath(fileName));
        QuaZipFileInfo::applyAttributesTo(
            srcInfo.filePath(), QuaZipFileInfo::NoAttr);
    }
    QVERIFY(compressDirOk);

    bool extractOk;
    if (useDevice) {
        QFile zipFile(zipPath);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        extractOk = !JlCompress::extractFiles(
            &zipFile, filesToExtract, targetDir.path())
                         .isEmpty();
    } else {
        extractOk =
            !JlCompress::extractFiles(zipPath, filesToExtract, targetDir.path())
                 .isEmpty();
    }

    struct Info {
        QFile::Permissions permissions;
        QFile::Permissions expectedPermissions;
        qint64 size;
        qint64 expectedSize;
    };

    QVector<Info> destInfo;

    for (int i = 0, count = filesToExtract.count(); i < count; i++) {
        const QString &fileName = filesToExtract.at(i);
        QFileInfo dstInfo(targetDir.filePath(fileName));
        QFileInfo srcInfo(dir.filePath(fileName));

        if (!dstInfo.exists())
            continue;

        destInfo.append({dstInfo.permissions(), permissions.at(i),
            dstInfo.size(), srcInfo.size()});
        QuaZipFileInfo::applyAttributesTo(
            dstInfo.filePath(), QuaZipFileInfo::NoAttr);
    }

    QVERIFY(extractOk);

    for (auto &info : destInfo) {
        QCOMPARE(info.permissions, info.expectedPermissions);
        QCOMPARE(info.size, info.expectedSize);
    }
}

void TestJlCompress::extractDir_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QString, fromDir);
    QADD_COLUMN(QStringList, expectedExtracted);
    QADD_COLUMN(bool, useDevice);

    QTest::newRow("from dir")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/test2sub.txt")
        << "testdir2"
        << (QStringList() << "testdir2/test2.txt"
                          << "testdir2/subdir"
                          << "testdir2/subdir/test2sub.txt")
        << false;
    QTest::newRow("separate dir") << (QStringList() << "laj/"
                                                    << "laj/lajfile.txt")
                                  << QString()
                                  << (QStringList() << "laj/"
                                                    << "laj/lajfile.txt")
                                  << true;
    QTest::newRow("FilePath Zip Slip")
        << (QStringList() << "test0.txt"
                          << "../zipslip.txt")
        << QString() << (QStringList() << "test0.txt") << false;
    QTest::newRow("Device Zip Slip")
        << (QStringList() << "test0.txt"
                          << "../zipslip.txt")
        << QString() << (QStringList() << "test0.txt") << true;
}

void TestJlCompress::extractDir()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QString, fromDir);
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
        extracted = JlCompress::extractDir(&zipFile, targetDir.path(), fromDir);
    } else {
        extracted = JlCompress::extractDir(zipPath, targetDir.path(), fromDir);
    }
    QCOMPARE(extracted.count(), expectedExtracted.count());
    for (const QString &fileName : expectedExtracted) {
        QString fullName = targetDir.filePath(fileName);
        QFileInfo dstInfo(fullName);
        QFileInfo srcInfo(dir.filePath(fileName));
        QCOMPARE(dstInfo.size(), srcInfo.size());
        QCOMPARE(dstInfo.permissions(), srcInfo.permissions());

        QString absolutePath =
            QDir::cleanPath(targetDir.absoluteFilePath(fileName));
        QVERIFY(extracted.contains(absolutePath));
    }
}
