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
#include "quazip/quazutils.h"
#include "quazip/quaziprawfileinfo.h"
#include "quazip/zip.h"
#include "quazip/unzip.h"
#include "quazip/private/quazipextrafields_p.h"

#include <QFile>
#include <QString>
#include <QStringList>
#include <QLocale>

void TestQuaZipFile::zipUnzip_data()
{
    QADD_COLUMN(QStringList, fileNames);
    QADD_COLUMN(QString, comment);
    QADD_COLUMN(QString, password);
    QADD_COLUMN(QByteArray, filePathCodec);
    QADD_COLUMN(QByteArray, commentCodec);
    QADD_COLUMN(QByteArray, passwordCodec);
    QADD_COLUMN(bool, zip64);
    QADD_COLUMN(int, size);

    QTest::newRow("simple") << (QStringList() << "test0.txt"
                                              << "testdir1/test1.txt"
                                              << "testdir2/test2.txt"
                                              << "testdir2/subdir/test2sub.txt")
                            << QString() << QString() << QByteArray()
                            << QByteArray() << QByteArray() << false << -1;

    QTest::newRow("dos only")
        << (QStringList() << QString::fromUtf8("dos_имя1.txt")
                          << QString::fromUtf8("папка/имя3.txt")
                          << QString::fromUtf8("subdir/dirпапка/файл8.txt"))
        << QString::fromUtf8("DOS Комментарий")
        << QString::fromUtf8("dos Пароль") << QByteArrayLiteral("IBM866")
        << QByteArrayLiteral("IBM866") << QByteArrayLiteral("IBM866") << false
        << -1;

    QTest::newRow("codecs+append")
        << (QStringList() << QString::fromUtf8(
                "русское имя файла с пробелами.txt"))
        << QString::fromUtf8("わたしはジップファイル")
        << QString::fromUtf8("password Юникод ユニコード")
        << QByteArrayLiteral("IBM866") << QByteArrayLiteral("EUC-JP")
        << QByteArrayLiteral("Shift_JIS") << false << -1;

    QTest::newRow("zip64+append")
        << (QStringList() << "test64.txt")
        << QString::fromUtf8("わたしはジップファイル") << QString("")
        << QByteArray() << QByteArray() << QByteArray() << true << -1;
    QTest::newRow("large enough to flush")
        << (QStringList() << QString::fromUtf8(
                "BAD CODEC FILE NAME わたしはジップファイル.bin"))
        << QString::fromUtf8("BAD CODEC COMMENT わたしはジップファイル")
        << QString() << QByteArrayLiteral("windows-1251")
        << QByteArrayLiteral("windows-1252") << QByteArray() << true
        << 65536 * 2;

    QTest::newRow("encrypted empty file")
        << QStringList("dummy") << QString("Zero encrypted")
        << QString("わたしはジップファイル") << QByteArray() << QByteArray()
        << QByteArrayLiteral("EUC-JP") << false << 0;
}

void TestQuaZipFile::zipUnzip()
{
    QFETCH(QStringList, fileNames);
    QFETCH(QString, comment);
    QFETCH(QString, password);
    QFETCH(QByteArray, filePathCodec);
    QFETCH(QByteArray, commentCodec);
    QFETCH(QByteArray, passwordCodec);
    QFETCH(bool, zip64);
    QFETCH(int, size);
    bool isText = size == -1;

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    QVERIFY2(createTestFiles(fileNames, size, filesPath),
        "Couldn't create test files for zipping");

    QFile zip(zipPath);

    QuaZip testZip(&zip);
    testZip.setZip64Enabled(zip64);
    QCOMPARE(testZip.isZip64Enabled(), zip64);

    if (!filePathCodec.isEmpty())
        testZip.setFilePathCodec(filePathCodec);
    if (!commentCodec.isEmpty())
        testZip.setCommentCodec(commentCodec);
    if (!passwordCodec.isEmpty())
        testZip.setPasswordCodec(passwordCodec);

    QCOMPARE(testZip.compatibility(), QuaZip::DefaultCompatibility);
    QuaZip::Compatibility compatibility;
    if (QLocale().language() == QLocale::Russian && filePathCodec == "IBM866" &&
        passwordCodec == commentCodec && commentCodec == filePathCodec) {
        compatibility = QuaZip::DosCompatible;
    } else if (!filePathCodec.isEmpty() && !commentCodec.isEmpty()) {
        compatibility = QuaZip::CustomCompatibility;
    } else {
        compatibility = QuaZip::DefaultCompatibility;
    }
    testZip.setCompatibility(compatibility);

    QFile::OpenMode fileOpenMode(QFile::ReadOnly);
    if (isText)
        fileOpenMode |= QFile::Text;
    QVERIFY(testZip.open(QuaZip::mdCreate));
    for (const QString &fileName : fileNames) {
        QFile inFile(dir.filePath(fileName));
        QVERIFY2(inFile.open(fileOpenMode), "Couldn't open input file");

        QuaZipFile outFile(&testZip);
        outFile.setFileInfo(
            QuaZipFileInfo::fromFile(inFile.fileName(), fileName));
        QCOMPARE(outFile.filePath(), fileName);
        outFile.setComment(comment);
        QCOMPARE(outFile.comment(), comment);
        if (!password.isNull()) {
            auto pwd = password;
            outFile.setPassword(&pwd);
            QVERIFY(pwd.isNull());
            QVERIFY(outFile.isEncrypted());
        }

        outFile.setIsText(isText);
        QCOMPARE(outFile.isText(), isText);
        QVERIFY(!outFile.isTextModeEnabled());
        QVERIFY(!outFile.open(QIODevice::ReadWrite));
        QVERIFY(!outFile.open(QIODevice::Append));
        QVERIFY(outFile.open(QIODevice::WriteOnly));
        QVERIFY(outFile.openMode() & QIODevice::Truncate);
        QVERIFY(outFile.openMode() & QIODevice::Unbuffered);
        QCOMPARE(outFile.isTextModeEnabled(), isText);
        auto data = inFile.readAll();
        QCOMPARE(outFile.write(data), qint64(data.length()));
        outFile.close();
        QCOMPARE(outFile.zipError(), ZIP_OK);
    }
    testZip.close();
    QCOMPARE(testZip.zipError(), ZIP_OK);

    QVERIFY(!zip.isOpen());
    QVERIFY(zip.open(QFile::ReadOnly));

    QVERIFY(testZip.open(QuaZip::mdUnzip));
    QCOMPARE(testZip.openMode(), QuaZip::mdUnzip);
    for (const QString &fileName : fileNames) {
        QFileInfo fileInfo(dir.filePath(fileName));

        QDateTime expectedCreationTime;
        QDateTime expectedModificationTime;
        QDateTime expectedAccessTime;
        if (compatibility == QuaZip::DosCompatible) {
            qint64 secs =
                (fileInfo.lastModified().toMSecsSinceEpoch() / 1000) & ~1;
            expectedCreationTime = expectedAccessTime =
                expectedModificationTime =
                    QDateTime::fromMSecsSinceEpoch(secs * 1000, Qt::UTC);
        } else {
            expectedCreationTime = fileInfo.created();
            expectedModificationTime = fileInfo.lastModified();
            expectedAccessTime = fileInfo.lastRead();
        }

        QFile original(fileInfo.filePath());
        QVERIFY(original.open(QFile::ReadOnly));

        QuaZipFile archived(&testZip);
        if (!password.isNull()) {
            auto pwd = password;
            archived.setPassword(&pwd);
            QVERIFY(pwd.isNull());
        }

        qint64 expectedFileSize = original.size();
        QString expectedFilePath;
        do {
            original.reset();
            archived.setFilePath(expectedFilePath, QuaZip::csInsensitive);

            QVERIFY(archived.open(QFile::ReadOnly));
            QVERIFY(archived.openMode() & QIODevice::Unbuffered);
            QVERIFY(!archived.isTextModeEnabled());
            QVERIFY(!archived.isSequential());
            QCOMPARE(archived.compressionMethod(), Z_DEFLATED);
            QCOMPARE(archived.compressionLevel(), Z_DEFAULT_COMPRESSION);
            QVERIFY(!archived.isRaw());
            QCOMPARE(archived.isEncrypted(), !password.isNull());
            QCOMPARE(archived.filePath(), expectedFilePath);
            QCOMPARE(archived.actualFilePath(), fileName);
            QCOMPARE(archived.comment(), comment);
            QCOMPARE(archived.centralExtraFields(),
                archived.fileInfo().centralExtraFields());
            QCOMPARE(archived.localExtraFields(),
                archived.fileInfo().localExtraFields());

            QCOMPARE(archived.creationTime(), expectedCreationTime);
            QCOMPARE(archived.modificationTime(), expectedModificationTime);
            QCOMPARE(archived.lastAccessTime(), expectedAccessTime);
            QCOMPARE(archived.permissions(), fileInfo.permissions());

            QCOMPARE(archived.size(), expectedFileSize);
            QCOMPARE(archived.bytesAvailable(), expectedFileSize);
            QCOMPARE(archived.atEnd(), expectedFileSize == 0);
            QVERIFY(archived.seek(expectedFileSize));
            QCOMPARE(archived.bytesAvailable(), qint64(0));
            QVERIFY(archived.atEnd());
            QCOMPARE(archived.pos(), expectedFileSize);
            QCOMPARE(archived.uncompressedSize(), expectedFileSize);
            QVERIFY(archived.compressedSize() > 0);
            QVERIFY(archived.reset());
            QCOMPARE(archived.atEnd(), expectedFileSize == 0);
            QCOMPARE(archived.peek(expectedFileSize),
                original.peek(expectedFileSize));
            QCOMPARE(archived.atEnd(), expectedFileSize == 0);
            QCOMPARE(archived.pos(), qint64(0));
            QCOMPARE(original.readAll(), archived.readAll());
            QVERIFY(archived.atEnd());
            QCOMPARE(archived.pos(), expectedFileSize);
            archived.close();
            QCOMPARE(archived.zipError(), UNZ_OK);

            if (!expectedFilePath.isEmpty())
                break;

            expectedFilePath = QLocale().toUpper(fileName);

            bool hasUnicodeFlag = 0 !=
                (archived.fileInfo().zipOptions() & QuaZipFileInfo::Unicode);
            if (compatibility == QuaZip::DosCompatible) {
                QVERIFY(!hasUnicodeFlag);
                QVERIFY(!archived.centralExtraFields().contains(
                    ZIPARCHIVE_EXTRA_FIELD_HEADER));
                QVERIFY(!archived.centralExtraFields().contains(
                    INFO_ZIP_UNICODE_PATH_HEADER));
                QVERIFY(!archived.localExtraFields().contains(
                    INFO_ZIP_UNICODE_PATH_HEADER));
                QVERIFY(!archived.centralExtraFields().contains(
                    INFO_ZIP_UNICODE_COMMENT_HEADER));
            } else {
                bool hasUnicodeExtra;
                bool hasZipArchiveExtra;

                if (compatibility == QuaZip::CustomCompatibility) {
                    hasZipArchiveExtra = !hasUnicodeFlag;
                    hasUnicodeExtra = hasZipArchiveExtra &&
                        (!testZip.filePathCodec()->canEncode(fileName) ||
                            !testZip.commentCodec()->canEncode(comment));
                } else {
                    hasUnicodeExtra = !hasUnicodeFlag &&
                        (!QuaZUtils::isAscii(fileName) ||
                            !QuaZUtils::isAscii(comment));
                    hasZipArchiveExtra = hasUnicodeExtra;
                }
                QCOMPARE(archived.centralExtraFields().contains(
                             ZIPARCHIVE_EXTRA_FIELD_HEADER),
                    hasZipArchiveExtra);
                QCOMPARE(archived.centralExtraFields().contains(
                             INFO_ZIP_UNICODE_PATH_HEADER),
                    hasUnicodeExtra);
                QCOMPARE(archived.localExtraFields().contains(
                             INFO_ZIP_UNICODE_PATH_HEADER),
                    hasUnicodeExtra);
                QCOMPARE(archived.centralExtraFields().contains(
                             INFO_ZIP_UNICODE_COMMENT_HEADER),
                    hasUnicodeExtra);
            }
        } while (true);
        testZip.goToNextFile();
    }
    if (!password.isNull()) {
        QVERIFY(testZip.goToFirstFile());
        QFile original(dir.filePath(testZip.currentFilePath()));
        QVERIFY(original.open(QIODevice::ReadOnly));
        QuaZipFile archived(&testZip);
        QString wrongPassword("WrongPassword");
        archived.setPassword(&wrongPassword);
        QVERIFY(archived.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll() != archived.readAll(), size != 0);
        archived.close();
        QVERIFY(archived.zipError() != UNZ_OK);
    }

    if (!password.isNull()) {
        QVERIFY(testZip.goToFirstFile());
        QFile original(dir.filePath(testZip.currentFilePath()));
        QVERIFY(original.open(QIODevice::ReadOnly));
        QuaZipFile archived(&testZip);
        auto pwd = testZip.passwordCodec()->fromUnicode(password);
        auto info = archived.fileInfo();
        info.setPassword(&pwd);
        QVERIFY(pwd.isNull());
        archived.setFileInfo(info);
        QVERIFY(archived.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), archived.readAll());
        QCOMPARE(archived.zipError(), UNZ_OK);
        archived.close();
        QCOMPARE(archived.zipError(), UNZ_OK);
    }
    testZip.close();
    QCOMPARE(testZip.zipError(), UNZ_OK);
}

void TestQuaZipFile::getZip()
{
    QuaZip testZip;
    QuaZipFile f1(&testZip);
    QCOMPARE(f1.zip(), &testZip);
    QVERIFY(f1.zipFilePath().isEmpty());
    QString zipPath("doesntexist.zip");
    QuaZipFile f2(zipPath, "someFile");
    QCOMPARE(f2.zipFilePath(), zipPath);
    QVERIFY(f2.zip() != nullptr);
    f2.setZip(&testZip);
    QCOMPARE(f2.zip(), &testZip);
    QVERIFY(f2.zipFilePath().isEmpty());
}

void TestQuaZipFile::setZipFilePath()
{
    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);

    QString testFileName = "testZipName.txt";
    QVERIFY(createTestFiles(QStringList() << testFileName, -1, filesPath));
    QVERIFY(createTestArchive(
        zipPath, QStringList() << testFileName, nullptr, filesPath));
    QuaZipFile testFile;
    testFile.setZipFilePath(zipPath);
    QVERIFY(testFile.zip() != nullptr);
    QCOMPARE(testFile.zipFilePath(), zipPath);
    testFile.setFilePath(testFileName);
    QVERIFY(testFile.open(QIODevice::ReadOnly));
    testFile.close();
    QCOMPARE(testFile.zipError(), UNZ_OK);
}

void TestQuaZipFile::zipFileInfo_data()
{
    QADD_COLUMN(QuaZip::CaseSensitivity, caseSensitivity);

    QTest::newRow("case sensitive") << QuaZip::csSensitive;
    QTest::newRow("case insensitive") << QuaZip::csInsensitive;
}

void TestQuaZipFile::zipFileInfo()
{
    QFETCH(QuaZip::CaseSensitivity, caseSensitivity);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);

    QString testFileName = "testZipName.txt";
    QString csFileName = testFileName;
    if (caseSensitivity == QuaZip::csInsensitive)
        csFileName = QLocale().toUpper(csFileName);

    QVERIFY(createTestFiles(QStringList() << testFileName, -1, filesPath));
    QVERIFY(createTestArchive(
        zipPath, QStringList() << testFileName, nullptr, filesPath));

    QuaZipFileInfo zipFileInfo, expectedInfo;
    {
        QuaZipFile testFile(zipPath, csFileName, caseSensitivity);
        QCOMPARE(testFile.filePath(), csFileName);
        QCOMPARE(testFile.caseSensitivity(), caseSensitivity);
        QVERIFY(testFile.open(QIODevice::ReadOnly));
        QCOMPARE(testFile.filePath(), csFileName);
        QCOMPARE(testFile.actualFilePath(), testFileName);
        zipFileInfo = testFile.fileInfo();
    }
    {
        QuaZip zip(zipPath);
        QVERIFY(zip.open(QuaZip::mdUnzip));
        QVERIFY(zip.setCurrentFile(testFileName, caseSensitivity));
        QVERIFY(zip.getCurrentFileInfo(expectedInfo));
    }
    QCOMPARE(zipFileInfo, expectedInfo);
}

void TestQuaZipFile::construct_data()
{
    QADD_COLUMN(QString, zipName);
    QADD_COLUMN(QString, fileName);
    QADD_COLUMN(QuaZip::CaseSensitivity, caseSensitivity);

    QTest::newRow("case sensitive")
        << QStringLiteral("zip_name.zip") << QStringLiteral("file_name.zip")
        << QuaZip::csSensitive;

    QTest::newRow("case insensitive")
        << QStringLiteral("zipName.zip") << QStringLiteral("fileName.zip")
        << QuaZip::csInsensitive;
}

void TestQuaZipFile::construct()
{
    QFETCH(QString, zipName);
    QFETCH(QString, fileName);
    QFETCH(QuaZip::CaseSensitivity, caseSensitivity);

    QuaZip testZip;
    QObject parent;
    {
        QuaZipFile zipFile(&parent);
        QVERIFY(zipFile.zip() == nullptr);
        QVERIFY(zipFile.zipFilePath().isEmpty());
        QVERIFY(zipFile.filePath().isEmpty());
        QCOMPARE(zipFile.caseSensitivity(), QuaZip::csDefault);
        QCOMPARE(zipFile.parent(), &parent);
    }
    {
        QuaZipFile zipFile(&testZip, &parent);
        QCOMPARE(zipFile.zip(), &testZip);
        QVERIFY(zipFile.zipFilePath().isEmpty());
        QVERIFY(zipFile.filePath().isEmpty());
        QCOMPARE(zipFile.caseSensitivity(), QuaZip::csDefault);
        QCOMPARE(zipFile.parent(), &parent);
    }
    {
        QuaZipFile zipFile(zipName, &parent);
        QVERIFY(zipFile.zip() != nullptr);
        QCOMPARE(zipFile.zipFilePath(), zipName);
        QVERIFY(zipFile.filePath().isEmpty());
        QCOMPARE(zipFile.caseSensitivity(), QuaZip::csDefault);
        QCOMPARE(zipFile.parent(), &parent);
    }
    {
        QuaZipFile zipFile(zipName, fileName, caseSensitivity, &parent);
        QVERIFY(zipFile.zip() != nullptr);
        QCOMPARE(zipFile.zipFilePath(), zipName);
        QCOMPARE(zipFile.filePath(), fileName);
        QCOMPARE(zipFile.caseSensitivity(), caseSensitivity);
        QCOMPARE(zipFile.parent(), &parent);
    }
    {
        QuaZipFile zipFile(&testZip, fileName, caseSensitivity, &parent);
        QCOMPARE(zipFile.zip(), &testZip);
        QVERIFY(zipFile.zipFilePath().isEmpty());
        QCOMPARE(zipFile.filePath(), fileName);
        QCOMPARE(zipFile.caseSensitivity(), caseSensitivity);
        QCOMPARE(zipFile.parent(), &parent);
    }
}

void TestQuaZipFile::fileAttributes_data()
{
    QADD_COLUMN(QuaZip::Compatibility, compatibility);
    QADD_COLUMN(QFile::Permissions, permissions);
    QADD_COLUMN(QuaZipFileInfo::Attributes, attributes);

    QTest::newRow("readonly")
        << QuaZip::Compatibility(QuaZip::DefaultCompatibility) << defaultRead
        << QuaZipFileInfo::Attributes(
               winFileArchivedAttr() | QuaZipFileInfo::ReadOnly);

    QTest::newRow("hidden")
        << QuaZip::Compatibility(QuaZip::DefaultCompatibility)
        << defaultReadWrite
        << QuaZipFileInfo::Attributes(
               winFileArchivedAttr() | QuaZipFileInfo::Hidden);

    QTest::newRow("dos/windows system hidden")
        << QuaZip::Compatibility(
               QuaZip::DosCompatible | QuaZip::WindowsCompatible)
        << defaultReadWrite
        << QuaZipFileInfo::Attributes(winFileArchivedAttr() |
               winFileSystemAttr() | QuaZipFileInfo::Hidden);

    QTest::newRow("windows hidden")
        << QuaZip::Compatibility(QuaZip::WindowsCompatible) << defaultReadWrite
        << QuaZipFileInfo::Attributes(QuaZipFileInfo::Hidden);

    QTest::newRow("unix hidden readonly")
        << QuaZip::Compatibility(QuaZip::UnixCompatible) << defaultRead
        << QuaZipFileInfo::Attributes(
               QuaZipFileInfo::Hidden | QuaZipFileInfo::ReadOnly);

    QTest::newRow("unix executable readonly")
        << QuaZip::Compatibility(QuaZip::UnixCompatible)
        << QFile::Permissions(defaultRead | execPermissions())
        << QuaZipFileInfo::Attributes(QuaZipFileInfo::ReadOnly);
}

void TestQuaZipFile::fileAttributes()
{
    QFETCH(QuaZip::Compatibility, compatibility);
    QFETCH(QFile::Permissions, permissions);
    QFETCH(QuaZipFileInfo::Attributes, attributes);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);

    QStringList testFiles;
    testFiles << "testZipName.txt";
    testFiles << QString::fromUtf8("test/dir/папка/");
    testFiles << QString::fromUtf8("папка/file.экс");

    if (attributes & QuaZipFileInfo::Hidden) {
        for (auto &fileName : testFiles) {
            QFileInfo fileInfo(fileName);
            bool isDir = false;
            if (fileInfo.fileName().isEmpty()) {
                fileInfo.setFile(fileInfo.path());
                isDir = true;
            }

            fileName = QDir::cleanPath(
                fileInfo.dir().filePath('.' + fileInfo.fileName()));
            if (isDir)
                fileName += '/';
        }
    }

    QVERIFY(createTestFiles(testFiles, -1, filesPath));

    if (compatibility & QuaZip::UnixCompatible) {
        QString symLinkName(
            (attributes & QuaZipFileInfo::Hidden) ? ".symlink" : "symlink");
        testFiles << symLinkName;

        QVERIFY(QuaZUtils::createSymLink(
            dir.filePath(symLinkName), testFiles.at(0)));
    }

    QVector<QFile::Permissions> perm;
    for (auto &fileName : testFiles) {
        auto testFilePath = dir.filePath(fileName);

        QuaZipFileInfo::applyAttributesTo(
            testFilePath, attributes, permissions);
        auto p = QFile::permissions(testFilePath);
        if (attributes & QuaZipFileInfo::ReadOnly)
            p &= ~defaultWrite;
        perm.append(p);
    }

    SaveDefaultZipOptions saveCompatibility;
    QuaZip::setDefaultCompatibility(compatibility);
    QVERIFY(createTestArchive(zipPath, testFiles, nullptr, filesPath));

    for (int i = 0, count = testFiles.count(); i < count; i++) {
        auto &fileName = testFiles.at(i);
        QuaZipFile zipFile(zipPath, fileName);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        bool isDir = fileName.endsWith('/');
        bool isSymLink = !isDir && fileName.contains("symlink");
        QCOMPARE(zipFile.isFile(), !isDir && !isSymLink);
        QCOMPARE(zipFile.isDir(), isDir);
        QCOMPARE(zipFile.isSymLink(), isSymLink);
        auto expectedSymLinkTarget = isSymLink ? testFiles.at(0) : QString();
        QCOMPARE(zipFile.symLinkTarget(), expectedSymLinkTarget);
        QCOMPARE(zipFile.permissions(), perm.at(i));

        auto expectedAttributes = attributes;
        if (isDir)
            expectedAttributes |= QuaZipFileInfo::DirAttr;

        QCOMPARE(zipFile.attributes(), expectedAttributes);
    }
}

void TestQuaZipFile::zip64()
{
    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);

    const int staticNumFiles = 2;
    QList<qint64> localOffsets;
    {
        QFile fakeLargeFile(zipPath);
        QVERIFY(fakeLargeFile.open(QIODevice::WriteOnly));
        QDataStream ds(&fakeLargeFile);
        ds.setByteOrder(QDataStream::LittleEndian);
        for (int i = 0; i < staticNumFiles; ++i) {
            localOffsets.append(fakeLargeFile.pos());
            QBuffer extra;
            extra.open(QIODevice::WriteOnly);
            QDataStream es(&extra);
            es.setByteOrder(QDataStream::LittleEndian);
            // prepare extra
            es << static_cast<quint16>(ZIP64_HEADER); // zip64
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
        for (int i = 0; i < staticNumFiles; ++i) {
            QBuffer extra;
            extra.open(QIODevice::WriteOnly);
            QDataStream es(&extra);
            es.setByteOrder(QDataStream::LittleEndian);
            // prepare extra
            es << static_cast<quint16>(ZIP64_HEADER); // zip64
            es << static_cast<quint16>(24); // extra data size
            es << static_cast<quint64>(0); // uncompressed size
            es << static_cast<quint64>(0); // compressed size
            es << static_cast<quint64>(localOffsets[i]);
            // now the central dir header
            ds << static_cast<quint32>(0x02014b50u); // central magic
            ds << static_cast<quint16>(45); // version made by
            ds << static_cast<quint16>(20); // version needed
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
        ds << static_cast<quint16>(20); // version needed
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
    }

    QByteArray text("tempo\niltempo");
    QString comment("Just another comment");
    QuaZip zip(zipPath);
    zip.setZip64Enabled(true);
    QVERIFY(zip.open(QuaZip::mdAdd));
    {
        QuaZipFile zipFile(&zip);
        zipFile.setFilePath("koko/add_test.txt");
        zipFile.setComment(comment);
        QVERIFY(!zipFile.open(QIODevice::ReadOnly));
        QVERIFY(!zipFile.open(QIODevice::ReadWrite));
        QVERIFY(!zipFile.open(QIODevice::Append));
        QVERIFY(zipFile.open(QIODevice::WriteOnly));
        QVERIFY(zipFile.openMode() & QIODevice::Truncate);
        QVERIFY(zipFile.openMode() & QIODevice::Unbuffered);
        QVERIFY(!zipFile.isTextModeEnabled());
        QCOMPARE(zipFile.write(text), text.length());
    }
    zip.close();

    int numFiles = staticNumFiles + 1;

    QVERIFY(zip.open(QuaZip::mdUnzip));
    QVERIFY(zip.goToFirstFile());
    for (int i = 0; i < numFiles; i++) {
        QuaZipRawFileInfo rawInfo;
        QVERIFY(zip.getCurrentRawFileInfo(rawInfo));
        bool containZip64Central = i < staticNumFiles;
        QCOMPARE(
            QuaZExtraField::toMap(rawInfo.centralExtra).contains(ZIP64_HEADER),
            containZip64Central);
        QVERIFY(
            QuaZExtraField::toMap(rawInfo.localExtra).contains(ZIP64_HEADER));
        QuaZipFile zipFile(&zip);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        qint64 expectedUncompressedSize =
            i < staticNumFiles ? 0 : text.length();
        QString expectedComment = i < staticNumFiles ? QString() : comment;
        QCOMPARE(zipFile.size(), expectedUncompressedSize);
        QCOMPARE(zipFile.uncompressedSize(), expectedUncompressedSize);
        QVERIFY(!zipFile.centralExtraFields().contains(ZIP64_HEADER));
        QVERIFY(!zipFile.localExtraFields().contains(ZIP64_HEADER));

        QCOMPARE(zipFile.comment(), expectedComment);

        zip.goToNextFile();
    }
}
