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

#include "testquazipdir.h"
#include "qztest.h"
#include "quazip/quazip.h"
#include "quazip/quazipdir.h"

Q_DECLARE_METATYPE(QDir::SortFlags)

void TestQuaZipDir::entryList_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QString, dirName);
    QADD_COLUMN(QStringList, nameFilters);
    QADD_COLUMN(QDir::Filters, filters);
    QADD_COLUMN(QDir::SortFlags, sorting);
    QADD_COLUMN(QStringList, entries);
    QADD_COLUMN(QuaZip::CaseSensitivity, caseSensitivity);

    QTest::newRow("simple+ignore hidden")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/test2sub.txt"
                          << "testdir2/.hidden")
        << "testdir2" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Unsorted)
        << (QStringList() << "test2.txt"
                          << "subdir")
        << QuaZip::csDefault;

    QTest::newRow("separate dir")
        << (QStringList() << "laj/"
                          << "laj/lajfile.txt")
        << "" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Unsorted) << (QStringList() << "laj")
        << QuaZip::csDefault;

    QTest::newRow("separate dir (subdir listing)")
        << (QStringList() << "laj/"
                          << "laj/lajfile.txt")
        << "laj" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Unsorted) << (QStringList() << "lajfile.txt")
        << QuaZip::csDefault;
    QTest::newRow("dirs only")
        << (QStringList() << "file"
                          << "dir/")
        << "" << QStringList() << QDir::Filters(QDir::Dirs)
        << QDir::SortFlags(QDir::Unsorted) << (QStringList() << "dir")
        << QuaZip::csDefault;
    QTest::newRow("files only")
        << (QStringList() << "file1"
                          << "parent/dir/"
                          << "parent/file2")
        << "parent" << QStringList() << QDir::Filters(QDir::Files)
        << QDir::SortFlags(QDir::Unsorted) << (QStringList() << "file2")
        << QuaZip::csDefault;
    QTest::newRow("sorted")
        << (QStringList() << "file1"
                          << "parent/subdir/"
                          << "parent/Subdir2/file3"
                          << "parent/file2"
                          << "parent/file0")
        << "parent" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name)
        << (QStringList() << "Subdir2"
                          << "file0"
                          << "file2"
                          << "subdir")
        << QuaZip::csDefault;
    QTest::newRow("sorted ignore case")
        << (QStringList() << "file1"
                          << "parent/subdir/"
                          << "parent/Subdir2/file3"
                          << "parent/file2"
                          << "parent/file0")
        << "parent" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name | QDir::IgnoreCase)
        << (QStringList() << "file0"
                          << "file2"
                          << "subdir"
                          << "Subdir2")
        << QuaZip::csDefault;
    QTest::newRow("sorted dirs first")
        << (QStringList() << "file1"
                          << "parent/subdir/"
                          << "parent/subdir2/file3"
                          << "parent/file2"
                          << "parent/file0")
        << "parent" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name | QDir::DirsFirst)
        << (QStringList() << "subdir"
                          << "subdir2"
                          << "file0"
                          << "file2")
        << QuaZip::csDefault;
    QTest::newRow("sorted dirs first reversed")
        << (QStringList() << "file1"
                          << "parent/subdir/"
                          << "parent/subdir2/file3"
                          << "parent/file2"
                          << "parent/file0")
        << "parent" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name | QDir::DirsFirst | QDir::Reversed)
        << (QStringList() << "subdir2"
                          << "subdir"
                          << "file2"
                          << "file0")
        << QuaZip::csDefault;
    QTest::newRow("sorted by size")
        << (QStringList() << "file10"
                          << "file1000")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Size)
        << (QStringList() << "file1000"
                          << "file10")
        << QuaZip::csDefault;
    QTest::newRow("sorted by time")
        << (QStringList() << "file04"
                          << "file03"
                          << "file02"
                          << "subdir/subfile")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Time)
        << (QStringList() << "subdir"
                          << "file02"
                          << "file03"
                          << "file04")
        << QuaZip::csDefault;
    QTest::newRow("sorted by type")
        << (QStringList() << "file1.txt"
                          << "file2.dat")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Type)
        << (QStringList() << "file2.dat"
                          << "file1.txt")
        << QuaZip::csDefault;
    QTest::newRow("name filter+hidden files")
        << (QStringList() << "File01"
                          << ".file02"
                          << "laj"
                          << "file_dir/")
        << "/" << QStringList("*file*")
        << QDir::Filters(QDir::Hidden | QDir::Files)
        << QDir::SortFlags(QDir::Name)
        << (QStringList() << ".file02"
                          << "File01")
        << QuaZip::csDefault;
    QTest::newRow("name filter case sensitive")
        << (QStringList() << "file01"
                          << "File02")
        << "/" << QStringList("*file*")
        << QDir::Filters(QDir::CaseSensitive | QDir::Files)
        << QDir::SortFlags(QDir::Name) << (QStringList() << "file01")
        << QuaZip::csDefault;
    QTest::newRow("modified only")
        << (QStringList() << "file01.mod"
                          << "original.file"
                          << "file02.mod")
        << "/" << QStringList("*file*")
        << QDir::Filters(QDir::Modified | QDir::Files)
        << QDir::SortFlags(QDir::Name)
        << (QStringList() << "file01.mod"
                          << "file02.mod")
        << QuaZip::csDefault;
    QTest::newRow("writable")
        << (QStringList() << "file01.txt"
                          << "readonly"
                          << "file02.txt")
        << "/" << QStringList() << QDir::Filters(QDir::Writable | QDir::Files)
        << QDir::SortFlags(QDir::Name)
        << (QStringList() << "file01.txt"
                          << "file02.txt")
        << QuaZip::csDefault;
    QTest::newRow("case sensitive")
        << (QStringList() << "a"
                          << "bb/"
                          << "B/")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name)
        << (QStringList() << "B"
                          << "a"
                          << "bb")
        << QuaZip::csSensitive;
    QTest::newRow("case sensitive+sort ignore case")
        << (QStringList() << "bb/"
                          << "a"
                          << "B/")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name | QDir::IgnoreCase)
        << (QStringList() << "a"
                          << "B"
                          << "bb")
        << QuaZip::csSensitive;
    QTest::newRow("case insensitive")
        << (QStringList() << "B"
                          << "b"
                          << "a")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name | QDir::IgnoreCase)
        << (QStringList() << "a"
                          << "B")
        << QuaZip::csInsensitive;
}

void TestQuaZipDir::entryList()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QString, dirName);
    QFETCH(QStringList, nameFilters);
    QFETCH(QDir::Filters, filters);
    QFETCH(QDir::SortFlags, sorting);
    QFETCH(QStringList, entries);
    QFETCH(QuaZip::CaseSensitivity, caseSensitivity);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir filesDir(filesPath);

    QVERIFY2(
        createTestFiles(fileNames,
            (filters != QDir::NoFilter && (filters & QDir::Modified)) ? 0 : -1,
            filesPath),
        "Couldn't create test files for zipping");
    for (auto &fileName : fileNames) {
        auto filePath = filesDir.filePath(fileName);
        QuaZipFileInfo::Attributes attr;
        {
            QFileInfo fileInfo(filePath);
            if (fileInfo.fileName().startsWith(".")) {
                attr |= QuaZipFileInfo::Hidden;
            }
        }
        if (fileName == "readonly") {
            attr |= QuaZipFileInfo::ReadOnly;
        } else if (fileName.endsWith(".mod")) {
            QTest::qWait(10);
            QFile file(filePath);
            QVERIFY(file.open(QIODevice::Append));
            QVERIFY(file.write(QByteArray(256, Qt::Uninitialized)) == 256);
        }
        QVERIFY(QuaZipFileInfo::applyAttributesTo(filePath, attr));
    }

    {
        auto filters2 = filters;
        if (filters2 == QDir::NoFilter)
            filters2 = QDir::AllEntries;
        filters2 |= QDir::NoDotAndDotDot;
        QStringList originalList;
        QDir dir(
            filesDir.filePath(dirName == QChar('/') ? QString('.') : dirName));
        if (filters2 & QDir::Modified) {
            auto list = dir.entryInfoList(nameFilters, filters2, sorting);
            for (const auto &entry : list) {
                if (entry.lastModified() > entry.created())
                    originalList.append(entry.fileName());
            }
        } else {
            originalList = dir.entryList(nameFilters, filters2, sorting);
        }
        if ((sorting & QDir::SortByMask) == QDir::Unsorted) {
            auto sorted = entries;
            qSort(sorted);
            qSort(originalList);
            QCOMPARE(sorted, originalList);
        } else {
            QCOMPARE(entries, originalList);
        }
    }
    QVERIFY2(createTestArchive(zipPath, fileNames, nullptr, filesPath),
        "Couldn't create test archive");

    QuaZip zip(zipPath);
    QVERIFY(zip.open(QuaZip::mdUnzip));
    QuaZipDir zipDir(&zip, dirName);
    QVERIFY(zipDir.exists());
    zipDir.setCaseSensitivity(caseSensitivity);
    QCOMPARE(zipDir.caseSensitivity(), caseSensitivity);
    QCOMPARE(zipDir.entryList(nameFilters, filters, sorting), entries);
    auto zipEntryInfoList = zipDir.entryInfoList(nameFilters, filters, sorting);
    QCOMPARE(zipEntryInfoList.count(), entries.count());
    for (int i = 0, count = zipEntryInfoList.count(); i < count; i++) {
        QCOMPARE(zipEntryInfoList.at(i).fileName(), entries.at(i));
    }
    // Test default sorting setting.
    zipDir.setSorting(sorting);
    QCOMPARE(zipDir.sorting(), sorting);
    QCOMPARE(zipDir.entryList(nameFilters, filters), entries);
    zipEntryInfoList = zipDir.entryInfoList(nameFilters, filters);
    QCOMPARE(zipEntryInfoList.count(), entries.count());
    for (int i = 0, count = zipEntryInfoList.count(); i < count; i++) {
        QCOMPARE(zipEntryInfoList.at(i).fileName(), entries.at(i));
    }
    // Test default name filter setting.
    zipDir.setNameFilters(nameFilters);
    QCOMPARE(zipDir.nameFilters(), nameFilters);
    QCOMPARE(zipDir.entryList(filters), entries);
    zipEntryInfoList = zipDir.entryInfoList(filters);
    QCOMPARE(zipEntryInfoList.count(), entries.count());
    for (int i = 0, count = zipEntryInfoList.count(); i < count; i++) {
        QCOMPARE(zipEntryInfoList.at(i).fileName(), entries.at(i));
    }
    // Test default filters.
    zipDir.setFilter(filters);
    QCOMPARE(zipDir.filter(), filters);
    auto zipEntryList = zipDir.entryList();
    QCOMPARE(zipEntryList, entries);
    QCOMPARE(zipEntryList.count(), zipDir.count());
    zipEntryInfoList = zipDir.entryInfoList();
    QCOMPARE(zipEntryInfoList.count(), entries.count());
    for (int i = 0, count = zipEntryInfoList.count(); i < count; i++) {
        QCOMPARE(zipEntryInfoList.at(i).fileName(), entries.at(i));
    }
    zip.close();
}

void TestQuaZipDir::cd_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QString, dirName);
    QADD_COLUMN(QString, targetDirName);
    QADD_COLUMN(QString, result);

    QTest::newRow("cdDown") << (QStringList() << "cddown.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << QString() << "testdir1"
                            << "testdir1";
    QTest::newRow("cdUp") << (QStringList() << "test0.txt"
                                            << "testdir1/test1.txt"
                                            << "testdir2/test2.txt"
                                            << "testdir2/subdir/test2sub.txt")
                          << "/testdir1"
                          << ".." << QString();
    QTest::newRow("cdSide") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << "/testdir1"
                            << "../testdir2"
                            << "testdir2";
    QTest::newRow("cdDownUp")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/test2sub.txt")
        << "/"
        << "testdir1/.." << QString();
    QTest::newRow("cdDeep") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << QString() << "testdir2/subdir"
                            << "testdir2/subdir";
    QTest::newRow("cdDeeper")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/subdir2/subdir3/test2sub.txt")
        << "/testdir2/subdir"
        << "subdir2/subdir3"
        << "testdir2/subdir/subdir2/subdir3";
    QTest::newRow("cdRoot") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << "/testdir1"
                            << "/" << QString();
}

void TestQuaZipDir::cd()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QString, dirName);
    QFETCH(QString, targetDirName);
    QFETCH(QString, result);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);

    QVERIFY2(createTestFiles(fileNames, -1, filesPath),
        "Couldn't create test files for zipping");

    QVERIFY2(createTestArchive(zipPath, fileNames, nullptr, filesPath),
        "Couldn't create test archive");

    QuaZip zip(zipPath);
    QVERIFY(zip.open(QuaZip::mdUnzip));
    auto origDirName = dirName;
    QuaZipDir dir(&zip, dirName);
    QVERIFY(dir.isValid());
    if (dirName.startsWith('/')) {
        dirName = dirName.mid(1);
    }
    if (dirName.endsWith('/')) {
        dirName.chop(1);
    }
    QCOMPARE(dir.path(), dirName);
    {
        int lastSlash = dirName.lastIndexOf('/');
        if (lastSlash < 0) {
            // dirName is just a single name
            if (dirName.isEmpty()) {
                QCOMPARE(dir.dirName(), QString::fromLatin1("."));
            } else {
                QCOMPARE(dir.dirName(), dirName);
            }
        } else {
            // dirName is a sequence
            QCOMPARE(dir.dirName(), dirName.mid(lastSlash + 1));
        }
    }
    if (targetDirName == "..") {
        QVERIFY(dir.cdUp());
    } else {
        QVERIFY(dir.cd(targetDirName));
    }
    QVERIFY(dir.isValid());
    QCOMPARE(dir.path(), result);
    // Try to go back
    QVERIFY(dir.cd(origDirName));
    QVERIFY(dir.isValid());
    QCOMPARE(dir.path(), dirName);
    dir.setPath(QString());
    QVERIFY(dir.isValid());
    QVERIFY(dir.path().isEmpty());
    dir.setPath(dirName);
    QCOMPARE(dir.path(), dirName);
    QVERIFY(dir.isValid());
    zip.close();
}

void TestQuaZipDir::operators()
{
    QStringList fileNames;
    fileNames << "dir/test.txt"
              << "root.txt";

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);

    QVERIFY2(createTestFiles(fileNames, -1, filesPath),
        "Couldn't create test files for zipping");

    QVERIFY2(createTestArchive(zipPath, fileNames, nullptr, filesPath),
        "Couldn't create test archive");

    QVERIFY(QDir(filesPath).removeRecursively());

    QuaZip zip(zipPath);

    QVERIFY(zip.open(QuaZip::mdUnzip));
    QuaZipDir dir(&zip, "dir");
    QVERIFY(dir.isValid());
    QCOMPARE(dir.path(), QString::fromLatin1("dir"));
    {
        QuaZipDir dir2;
        QVERIFY(!dir2.isValid());
        QVERIFY(dir2 != dir); // opertor!=
        dir2 = dir; // operator=()
        QVERIFY(dir2.isValid());
        QCOMPARE(dir2.zip(), dir.zip());
        QCOMPARE(dir2.path(), QString::fromLatin1("dir"));
        dir2.cdUp();
        QVERIFY(dir2.path().isEmpty());
        QCOMPARE(dir.path(), QString::fromLatin1("dir"));
    }
    {
        QuaZipDir dir2(dir); // operator=()
        QVERIFY(dir2.isValid());
        QCOMPARE(dir2.path(), QString::fromLatin1("dir"));
        QCOMPARE(dir2, dir); // opertor==

        dir2.setPath("..");
        QVERIFY(!dir2.isValid());
    }
    dir.cd("/");
    QCOMPARE(dir[0], QString::fromLatin1("dir"));
    QCOMPARE(dir[1], QString::fromLatin1("root.txt"));
    zip.close();
}

void TestQuaZipDir::filePath()
{
    QStringList fileNames;
    fileNames << "root.txt"
              << "dir/test.txt";

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);

    QVERIFY2(createTestFiles(fileNames, -1, filesPath),
        "Couldn't create test files for zipping");

    QVERIFY2(createTestArchive(zipPath, fileNames, nullptr, filesPath),
        "Couldn't create test archive");

    QVERIFY(QDir(filesPath).removeRecursively());

    QuaZip zip(zipPath);
    QVERIFY(zip.open(QuaZip::mdUnzip));
    QuaZipDir dir(&zip);
    QVERIFY(dir.cd("dir"));
    QCOMPARE(
        dir.relativeFilePath("/root.txt"), QString::fromLatin1("../root.txt"));
    QCOMPARE(dir.filePath("test.txt"), QString::fromLatin1("dir/test.txt"));
    zip.close();
}
