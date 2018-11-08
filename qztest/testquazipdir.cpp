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

    QTest::newRow("simple")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/test2sub.txt")
        << "testdir2" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Unsorted)
        << (QStringList() << "test2.txt"
                          << "subdir/")
        << QuaZip::csDefault;

    QTest::newRow("separate dir")
        << (QStringList() << "laj/"
                          << "laj/lajfile.txt")
        << "" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Unsorted) << (QStringList() << "laj/")
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
        << QDir::SortFlags(QDir::Unsorted) << (QStringList() << "dir/")
        << QuaZip::csDefault;
    QTest::newRow("files only")
        << (QStringList() << "file1"
                          << "parent/dir/"
                          << "parent/file2")
        << "parent" << QStringList() << QDir::Filters(QDir::Files)
        << QDir::Filters(QDir::Unsorted) << (QStringList() << "file2")
        << QuaZip::csDefault;
    QTest::newRow("sorted")
        << (QStringList() << "file1"
                          << "parent/subdir/"
                          << "parent/subdir2/file3"
                          << "parent/file2"
                          << "parent/file0")
        << "parent" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name)
        << (QStringList() << "file0"
                          << "file2"
                          << "subdir/"
                          << "subdir2/")
        << QuaZip::csDefault;
    QTest::newRow("sorted dirs first")
        << (QStringList() << "file1"
                          << "parent/subdir/"
                          << "parent/subdir2/file3"
                          << "parent/file2"
                          << "parent/file0")
        << "parent" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name | QDir::DirsFirst)
        << (QStringList() << "subdir/"
                          << "subdir2/"
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
        << (QStringList() << "subdir2/"
                          << "subdir/"
                          << "file2"
                          << "file0")
        << QuaZip::csDefault;
    QTest::newRow("sorted by size")
        << (QStringList() << "file000"
                          << "file10")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Size)
        << (QStringList() << "file10"
                          << "file000")
        << QuaZip::csDefault;
    QTest::newRow("sorted by time")
        << (QStringList() << "file04"
                          << "file03"
                          << "file02"
                          << "subdir/subfile")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Time)
        << (QStringList() << "subdir/"
                          << "file04"
                          << "file02"
                          << "file03")
        << QuaZip::csDefault;
    QTest::newRow("sorted by type")
        << (QStringList() << "file1.txt"
                          << "file2.dat")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Type)
        << (QStringList() << "file2.dat"
                          << "file1.txt")
        << QuaZip::csDefault;
    QTest::newRow("name filter")
        << (QStringList() << "file01"
                          << "file02"
                          << "laj")
        << "/" << QStringList("file*") << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name)
        << (QStringList() << "file01"
                          << "file02")
        << QuaZip::csDefault;
    QTest::newRow("case sensitive")
        << (QStringList() << "a"
                          << "B")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name)
        << (QStringList() << "B"
                          << "a")
        << QuaZip::csSensitive;
    QTest::newRow("case insensitive")
        << (QStringList() << "B"
                          << "a")
        << "/" << QStringList() << QDir::Filters(QDir::NoFilter)
        << QDir::SortFlags(QDir::Name)
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

    QVERIFY2(createTestFiles(fileNames, -1, filesPath),
        "Couldn't create test files for zipping");

    QVERIFY2(createTestArchive(zipPath, fileNames, nullptr, filesPath),
        "Couldn't create test archive");

    QVERIFY(QDir(filesPath).removeRecursively());

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
        QCOMPARE(zipEntryInfoList.at(i).filePath(), entries.at(i));
    }
    // Test default sorting setting.
    zipDir.setSorting(sorting);
    QCOMPARE(zipDir.sorting(), sorting);
    QCOMPARE(zipDir.entryList(nameFilters, filters), entries);
    zipEntryInfoList = zipDir.entryInfoList(nameFilters, filters);
    QCOMPARE(zipEntryInfoList.count(), entries.count());
    for (int i = 0, count = zipEntryInfoList.count(); i < count; i++) {
        QCOMPARE(zipEntryInfoList.at(i).filePath(), entries.at(i));
    }
    // Test default name filter setting.
    zipDir.setNameFilters(nameFilters);
    QCOMPARE(zipDir.nameFilters(), nameFilters);
    QCOMPARE(zipDir.entryList(filters), entries);
    zipEntryInfoList = zipDir.entryInfoList(filters);
    QCOMPARE(zipEntryInfoList.count(), entries.count());
    for (int i = 0, count = zipEntryInfoList.count(); i < count; i++) {
        QCOMPARE(zipEntryInfoList.at(i).filePath(), entries.at(i));
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
        QCOMPARE(zipEntryInfoList.at(i).filePath(), entries.at(i));
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
                            << ""
                            << "testdir1"
                            << "testdir1";
    QTest::newRow("cdUp") << (QStringList() << "test0.txt"
                                            << "testdir1/test1.txt"
                                            << "testdir2/test2.txt"
                                            << "testdir2/subdir/test2sub.txt")
                          << "testdir1"
                          << ".."
                          << "";
    QTest::newRow("cdSide") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << "testdir1"
                            << "../testdir2"
                            << "testdir2";
    QTest::newRow("cdDownUp")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/test2sub.txt")
        << ""
        << "testdir1/.."
        << "";
    QTest::newRow("cdDeep") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << ""
                            << "testdir2/subdir"
                            << "testdir2/subdir";
    QTest::newRow("cdDeeper")
        << (QStringList() << "test0.txt"
                          << "testdir1/test1.txt"
                          << "testdir2/test2.txt"
                          << "testdir2/subdir/subdir2/subdir3/test2sub.txt")
        << "testdir2/subdir"
        << "subdir2/subdir3"
        << "testdir2/subdir/subdir2/subdir3";
    QTest::newRow("cdRoot") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << "testdir1"
                            << "/"
                            << "";
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

    QVERIFY(QDir(filesPath).removeRecursively());

    QuaZip zip(zipPath);
    QVERIFY(zip.open(QuaZip::mdUnzip));
    QuaZipDir dir(&zip, dirName);
    if (dirName.startsWith('/')) {
        dirName = dirName.mid(1);
    }
    if (dirName.endsWith('/')) {
        dirName.chop(1);
    }
    QCOMPARE(dir.path(), '/' + dirName);
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
            QCOMPARE(dir.dirName(), dirName.right(lastSlash + 1));
        }
    }
    if (targetDirName == "..") {
        QVERIFY(dir.cdUp());
    } else {
        QVERIFY(dir.cd(targetDirName));
    }
    QCOMPARE(dir.path(), result);
    // Try to go back
    dir.setPath(dirName);
    QCOMPARE(dir.path(), '/' + dirName);
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
    QVERIFY(!dir.isValid());
    QCOMPARE(dir.path(), QString::fromLatin1("/dir"));
    {
        QuaZipDir dir2;
        QVERIFY(!dir2.isValid());
        QVERIFY(dir2 != dir); // opertor!=
        dir2 = dir; // operator=()
        QVERIFY(dir2.isValid());
        QCOMPARE(dir2.zip(), dir.zip());
        QCOMPARE(dir2.path(), QString::fromLatin1("/dir"));
        dir2.cdUp();
        QCOMPARE(dir2.path(), QString::fromLatin1("/"));
        QCOMPARE(dir.path(), QString::fromLatin1("/dir"));
    }
    {
        QuaZipDir dir2(dir); // operator=()
        QVERIFY(dir2.isValid());
        QCOMPARE(dir2.path(), QString::fromLatin1("/dir"));
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
    QCOMPARE(dir.filePath("test.txt"), QString::fromLatin1("/dir/test.txt"));
    zip.close();
}
