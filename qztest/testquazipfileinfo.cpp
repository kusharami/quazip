/*
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

#include "testquazipfileinfo.h"

#include "qztest.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include "quazip/quazip.h"
#include "quazip/quazipfile.h"
#include "quazip/quazipfileinfo.h"
#include "quazip/quacrc32.h"
#include "quazip/quazutils.h"
#include "quazip/private/quazipextrafields_p.h"

#include <zlib.h>

TestQuaZipFileInfo::TestQuaZipFileInfo(QObject *parent)
    : QObject(parent)
{
}

void TestQuaZipFileInfo::fromFile_data()
{
    QADD_COLUMN(QFile::Permissions, permissions);
    QADD_COLUMN(QuaZipFileInfo::Attributes, attributes);

    QTest::newRow("system")
        << defaultReadWrite << QuaZipFileInfo::Attributes(winFileSystemAttr());

    QTest::newRow("hidden")
        << defaultReadWrite
        << QuaZipFileInfo::Attributes(
               winFileArchivedAttr() | QuaZipFileInfo::Hidden);

    QTest::newRow("readonly")
        << defaultRead
        << QuaZipFileInfo::Attributes(
               winFileArchivedAttr() | QuaZipFileInfo::ReadOnly);

    QTest::newRow("executable")
        << QFile::Permissions(defaultRead | execPermissions())
        << QuaZipFileInfo::Attributes(QuaZipFileInfo::ReadOnly);
}

void TestQuaZipFileInfo::fromFile()
{
    QFETCH(QFile::Permissions, permissions);
    QFETCH(QuaZipFileInfo::Attributes, attributes);

    QString fileName("temp.bin");
    if (attributes & QuaZipFileInfo::Hidden)
        fileName = '.' + fileName;

    SaveDefaultZipOptions saveCompatibility;

    QTemporaryDir tempDir;
    auto filesPath = tempFilesPath(tempDir);
    auto filePath = QDir(filesPath).filePath(fileName);

    QVERIFY(createTestFiles({fileName}, -1, filesPath));

    bool applyAttributes =
        QuaZipFileInfo::applyAttributesTo(filePath, attributes, permissions);
    if (!applyAttributes) {
        qWarning("Some attributes or permissions are not applied");
    }

    auto zipFileInfo = QuaZipFileInfo::fromFile(filePath);
    QVERIFY(zipFileInfo.isFile());
    QVERIFY(!zipFileInfo.isDir());
    QVERIFY(!zipFileInfo.isRaw());
    QVERIFY(!zipFileInfo.isText());
    QVERIFY(!zipFileInfo.isSymLink());
    QVERIFY(!zipFileInfo.isEncrypted());
    QVERIFY(zipFileInfo.comment().isEmpty());
    QVERIFY(zipFileInfo.centralExtraFields().isEmpty());
    QVERIFY(zipFileInfo.localExtraFields().isEmpty());
    QVERIFY(zipFileInfo.symLinkTarget().isEmpty());
    QVERIFY(zipFileInfo.path().isEmpty());

    QCOMPARE(zipFileInfo.entryType(), QuaZipFileInfo::File);
    QCOMPARE(zipFileInfo.zipOptions(), QuaZipFileInfo::NormalCompression);
    QCOMPARE(zipFileInfo.compressedSize(), qint64(0));
    QCOMPARE(zipFileInfo.compressionMethod(), quint16(Z_DEFLATED));
    QCOMPARE(zipFileInfo.compressionStrategy(), quint16(Z_DEFAULT_STRATEGY));
    QCOMPARE(zipFileInfo.compressionLevel(), Z_DEFAULT_COMPRESSION);
    QCOMPARE(zipFileInfo.crc(), quint32(0));
    QCOMPARE(zipFileInfo.zipVersionNeeded(), quint16(10));
    QCOMPARE(zipFileInfo.zipVersionMadeBy(), quint8(10));
    QCOMPARE(zipFileInfo.madeBy(), quint16(10));
    QCOMPARE(zipFileInfo.systemMadeBy(), QuaZipFileInfo::OS_MSDOS);
    QCOMPARE(zipFileInfo.diskNumber(), 0);
    QCOMPARE(zipFileInfo.internalAttributes(), quint16(0));
    QCOMPARE(zipFileInfo.attributes(), attributes);
    QCOMPARE(quint16(zipFileInfo.externalAttributes()), quint16(attributes));

    QCOMPARE(zipFileInfo.isArchived(),
        attributes.testFlag(QuaZipFileInfo::Archived));
    QCOMPARE(
        zipFileInfo.isHidden(), attributes.testFlag(QuaZipFileInfo::Hidden));
    QCOMPARE(
        zipFileInfo.isSystem(), attributes.testFlag(QuaZipFileInfo::System));
    QCOMPARE(zipFileInfo.isWritable(),
        !attributes.testFlag(QuaZipFileInfo::ReadOnly));

    QCOMPARE(zipFileInfo.isExecutable(),
        0 !=
            (permissions &
                (QFile::ExeGroup | QFile::ExeOther | QFile::ExeUser |
                    QFile::ExeOwner)));

    QFileInfo fileInfo(filePath);

    QCOMPARE(zipFileInfo.uncompressedSize(), fileInfo.size());
    QCOMPARE(zipFileInfo.fileName(), fileInfo.fileName());
    QCOMPARE(zipFileInfo.filePath(), fileInfo.fileName());
    QCOMPARE(zipFileInfo.modificationTime(), fileInfo.lastModified());
    QCOMPARE(zipFileInfo.lastAccessTime(), fileInfo.lastRead());
    QCOMPARE(zipFileInfo.creationTime(), fileInfo.created());
    QCOMPARE(zipFileInfo.permissions(), fileInfo.permissions());

    QuaZipFileInfo checkZipFileInfo;
    checkZipFileInfo.initWithFile(fileInfo);
    QCOMPARE(zipFileInfo, checkZipFileInfo);
    checkZipFileInfo.initWithFile(filePath);
    QCOMPARE(zipFileInfo, checkZipFileInfo);
}

void TestQuaZipFileInfo::fromDir_data()
{
    QADD_COLUMN(QFile::Permissions, permissions);
    QADD_COLUMN(QuaZipFileInfo::Attributes, attributes);

    QTest::newRow("writable")
        << QStringLiteral("writable") << defaultReadWrite
        << QuaZipFileInfo::Attributes(
               QuaZipFileInfo::Archived | winFileSystemAttr());

    QTest::newRow("hidden")
        << QStringLiteral(".hidden") << defaultReadWrite
        << QuaZipFileInfo::Attributes(QuaZipFileInfo::Hidden);

    QTest::newRow("readonly")
        << QStringLiteral("readonly") << defaultRead
        << QuaZipFileInfo::Attributes(
               winFileArchivedAttr() | QuaZipFileInfo::ReadOnly);
}

void TestQuaZipFileInfo::fromDir()
{
    QFETCH(QFile::Permissions, permissions);
    QFETCH(QuaZipFileInfo::Attributes, attributes);

    attributes |= QuaZipFileInfo::DirAttr;

    QTemporaryDir tempDir;
    QDir dir(tempFilesPath(tempDir));
    QVERIFY(QDir(tempDir.path()).mkdir(dir.dirName()));

    bool applyAttributes =
        QuaZipFileInfo::applyAttributesTo(dir.path(), attributes, permissions);
    if (!applyAttributes) {
        qWarning("Some attributes or permissions are not applied");
    }

    auto zipFileInfo = QuaZipFileInfo::fromDir(dir);
    QVERIFY(zipFileInfo.isDir());
    QVERIFY(!zipFileInfo.isFile());
    QVERIFY(!zipFileInfo.isRaw());
    QVERIFY(!zipFileInfo.isText());
    QVERIFY(!zipFileInfo.isSymLink());
    QVERIFY(!zipFileInfo.isEncrypted());
    QVERIFY(!zipFileInfo.isExecutable());
    QVERIFY(zipFileInfo.comment().isEmpty());
    QVERIFY(zipFileInfo.centralExtraFields().isEmpty());
    QVERIFY(zipFileInfo.localExtraFields().isEmpty());
    QVERIFY(zipFileInfo.symLinkTarget().isEmpty());
    QVERIFY(zipFileInfo.path().isEmpty());

    QCOMPARE(zipFileInfo.entryType(), QuaZipFileInfo::Directory);
    QCOMPARE(zipFileInfo.zipOptions(), QuaZipFileInfo::NormalCompression);
    QCOMPARE(zipFileInfo.crc(), quint32(0));
    QCOMPARE(zipFileInfo.compressedSize(), qint64(0));
    QCOMPARE(zipFileInfo.uncompressedSize(), qint64(0));
    QCOMPARE(zipFileInfo.compressionMethod(), quint16(Z_DEFLATED));
    QCOMPARE(zipFileInfo.compressionStrategy(), quint16(Z_DEFAULT_STRATEGY));
    QCOMPARE(zipFileInfo.compressionLevel(), Z_DEFAULT_COMPRESSION);
    QCOMPARE(zipFileInfo.zipVersionNeeded(), quint16(10));
    QCOMPARE(zipFileInfo.zipVersionMadeBy(), quint8(10));
    QCOMPARE(zipFileInfo.madeBy(), quint16(10));
    QCOMPARE(zipFileInfo.systemMadeBy(), QuaZipFileInfo::OS_MSDOS);
    QCOMPARE(zipFileInfo.diskNumber(), 0);
    QCOMPARE(zipFileInfo.internalAttributes(), quint16(0));
    QCOMPARE(zipFileInfo.attributes(), attributes);
    QCOMPARE(quint16(zipFileInfo.externalAttributes()), quint16(attributes));

    QCOMPARE(zipFileInfo.isArchived(),
        attributes.testFlag(QuaZipFileInfo::Archived));
    QCOMPARE(
        zipFileInfo.isHidden(), attributes.testFlag(QuaZipFileInfo::Hidden));
    QCOMPARE(
        zipFileInfo.isSystem(), attributes.testFlag(QuaZipFileInfo::System));
    QCOMPARE(zipFileInfo.isWritable(),
        !attributes.testFlag(QuaZipFileInfo::ReadOnly));

    QCOMPARE(zipFileInfo.fileName(), dir.dirName());
    QCOMPARE(zipFileInfo.filePath(), dir.dirName() + '/');

    QFileInfo dirInfo(dir.path());
    QCOMPARE(zipFileInfo.modificationTime(), dirInfo.lastModified());
    QCOMPARE(zipFileInfo.lastAccessTime(), dirInfo.lastRead());
    QCOMPARE(zipFileInfo.creationTime(), dirInfo.created());
    QCOMPARE(zipFileInfo.permissions(), dirInfo.permissions());

    QuaZipFileInfo checkZipFileInfo;
    checkZipFileInfo.initWithDir(dir);
    QCOMPARE(zipFileInfo, checkZipFileInfo);
    checkZipFileInfo.initWithFile(dirInfo);
    QCOMPARE(zipFileInfo, checkZipFileInfo);
    checkZipFileInfo.initWithFile(dir.path());
    QCOMPARE(zipFileInfo, checkZipFileInfo);
}

void TestQuaZipFileInfo::fromLink_data()
{
    QADD_COLUMN(bool, linkToDir);
    QADD_COLUMN(QString, linkFileName);
    QADD_COLUMN(QString, targetFileName);
    QADD_COLUMN(QFile::Permissions, permissions);
    QADD_COLUMN(QuaZipFileInfo::Attributes, attributes);

    QTest::newRow("file_link")
        << false << QStringLiteral("file_link") << QStringLiteral("file.txt")
        << defaultReadWrite
        << QuaZipFileInfo::Attributes(
               winFileArchivedAttr() | winFileSystemAttr());

    QTest::newRow("dir_link")
        << true << QStringLiteral(".hidden_dir_link") << QStringLiteral("dir")
        << defaultRead
        << QuaZipFileInfo::Attributes(
               QuaZipFileInfo::Hidden | QuaZipFileInfo::ReadOnly);
}

void TestQuaZipFileInfo::fromLink()
{
    QFETCH(bool, linkToDir);
    QFETCH(QString, linkFileName);
    QFETCH(QString, targetFileName);
    QFETCH(QFile::Permissions, permissions);
    QFETCH(QuaZipFileInfo::Attributes, attributes);

    QTemporaryDir tempDir;
    QDir dir(tempFilesPath(tempDir));
    QVERIFY(QDir(tempDir.path()).mkdir(dir.dirName()));

    if (linkToDir) {
        QVERIFY(dir.mkdir(targetFileName));
    } else {
        QVERIFY(createTestFiles({targetFileName}, -1, dir.path()));
    }

    QString linkFilePath = QDir(tempDir.path()).filePath(linkFileName);
    QVERIFY(
        QuaZUtils::createSymLink(linkFilePath, dir.filePath(targetFileName)));

    bool applyAttributes = QuaZipFileInfo::applyAttributesTo(
        linkFilePath, attributes, permissions);
    if (!applyAttributes) {
        qWarning("Some attributes or permissions are not applied");
    }

    auto zipFileInfo = QuaZipFileInfo::fromFile(linkFilePath);
    QVERIFY(zipFileInfo.isSymLink());
    QVERIFY(!zipFileInfo.isDir());
    QVERIFY(!zipFileInfo.isFile());
    QVERIFY(!zipFileInfo.isRaw());
    QVERIFY(!zipFileInfo.isText());
    QVERIFY(!zipFileInfo.isEncrypted());
    QVERIFY(!zipFileInfo.isExecutable());
    QVERIFY(zipFileInfo.comment().isEmpty());
    QVERIFY(zipFileInfo.centralExtraFields().isEmpty());
    QVERIFY(zipFileInfo.localExtraFields().isEmpty());
    QVERIFY(zipFileInfo.path().isEmpty());

    QCOMPARE(zipFileInfo.symLinkTarget(), targetFileName);
    QCOMPARE(zipFileInfo.entryType(), QuaZipFileInfo::SymLink);
    QCOMPARE(zipFileInfo.zipOptions(), QuaZipFileInfo::NormalCompression);
    QCOMPARE(zipFileInfo.crc(), quint32(0));
    QCOMPARE(zipFileInfo.compressedSize(), qint64(0));
    QCOMPARE(zipFileInfo.uncompressedSize(), qint64(0));
    QCOMPARE(zipFileInfo.compressionMethod(), quint16(Z_DEFLATED));
    QCOMPARE(zipFileInfo.compressionStrategy(), quint16(Z_DEFAULT_STRATEGY));
    QCOMPARE(zipFileInfo.compressionLevel(), Z_DEFAULT_COMPRESSION);
    QCOMPARE(zipFileInfo.zipVersionNeeded(), quint16(10));
    QCOMPARE(zipFileInfo.zipVersionMadeBy(), quint8(10));
    QCOMPARE(zipFileInfo.madeBy(), quint16(10));
    QCOMPARE(zipFileInfo.systemMadeBy(), QuaZipFileInfo::OS_UNIX);
    QCOMPARE(zipFileInfo.diskNumber(), 0);
    QCOMPARE(zipFileInfo.internalAttributes(), quint16(0));
    QCOMPARE(zipFileInfo.attributes(), attributes);
    QCOMPARE(quint16(zipFileInfo.externalAttributes()), quint16(attributes));

    QCOMPARE(zipFileInfo.isArchived(),
        attributes.testFlag(QuaZipFileInfo::Archived));
    QCOMPARE(
        zipFileInfo.isHidden(), attributes.testFlag(QuaZipFileInfo::Hidden));
    QCOMPARE(
        zipFileInfo.isSystem(), attributes.testFlag(QuaZipFileInfo::System));
    QCOMPARE(zipFileInfo.isWritable(),
        !attributes.testFlag(QuaZipFileInfo::ReadOnly));

    QFileInfo linkInfo(linkFilePath);

    QCOMPARE(zipFileInfo.fileName(), linkInfo.fileName());
    QCOMPARE(zipFileInfo.filePath(), linkFileName);
    QCOMPARE(zipFileInfo.modificationTime(), linkInfo.lastModified());
    QCOMPARE(zipFileInfo.lastAccessTime(), linkInfo.lastRead());
    QCOMPARE(zipFileInfo.creationTime(), linkInfo.created());
    QCOMPARE(zipFileInfo.permissions(), linkInfo.permissions());

    QuaZipFileInfo checkZipFileInfo;
    checkZipFileInfo.initWithFile(linkInfo);
    QCOMPARE(zipFileInfo, checkZipFileInfo);
    checkZipFileInfo.initWithFile(linkFilePath);
    QCOMPARE(zipFileInfo, checkZipFileInfo);
}

void TestQuaZipFileInfo::fromZipFile_data()
{
    QADD_COLUMN(QString, fileName);
    QADD_COLUMN(int, fileSize);
    QADD_COLUMN(QString, password);
    QADD_COLUMN(QuaZip::Compatibility, compatibility);

    QTest::newRow("simple")
        << QStringLiteral("simple.bin") << 10 << QString()
        << QuaZip::Compatibility(QuaZip::CustomCompatibility);

    QTest::newRow("unix_windows")
        << QString::fromUtf8("факел.bin") << 9 << QString()
        << QuaZip::Compatibility(
               QuaZip::UnixCompatible | QuaZip::WindowsCompatible);

    QTest::newRow("text_unix_only")
        << QString::fromUtf8("бублик.txt") << -1 << QString()
        << QuaZip::Compatibility(QuaZip::UnixCompatible);

    QTest::newRow("text_dos_only")
        << QStringLiteral("dos.txt") << -1 << QString()
        << QuaZip::Compatibility(QuaZip::DosCompatible);

    QTest::newRow("text_dos_compatible")
        << QString::fromUtf8("бублик.txt") << -1 << QString()
        << QuaZip::Compatibility(QuaZip::DosCompatible |
               QuaZip::UnixCompatible | QuaZip::WindowsCompatible);

    QTest::newRow("encrypted_windows_only")
        << QStringLiteral("encrypted.bin") << 12
        << QStringLiteral("My babushka's birthday")
        << QuaZip::Compatibility(QuaZip::WindowsCompatible);
}

void TestQuaZipFileInfo::fromZipFile()
{
    QFETCH(QString, fileName);
    QFETCH(int, fileSize);
    QFETCH(QString, password);
    QFETCH(QuaZip::Compatibility, compatibility);
    bool isText = fileName.endsWith(".txt", Qt::CaseInsensitive);

    SaveDefaultZipOptions saveCompatibility;
    QuaZip::setDefaultCompatibility(compatibility);

    QTemporaryDir tempDir;
    auto zipPath = tempZipPath(tempDir);
    auto filesPath = tempFilesPath(tempDir);
    QDir dir(filesPath);
    {
        QStringList testFiles;
        testFiles << fileName;
        QVERIFY(createTestFiles(testFiles, fileSize, filesPath));
        QVERIFY(createTestArchive(
            zipPath, testFiles, nullptr, password, filesPath));
    }

    QuaZipFileInfo zipFileInfo;
    {
        QuaZipFile zipFile(zipPath, fileName);
        auto pwd = password;
        zipFile.setPassword(&pwd);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        zipFileInfo = zipFile.fileInfo();
        zipFile.close();
        QCOMPARE(zipFile.zipError(), 0);
    }
    QCOMPARE(zipFileInfo.diskNumber(), 0);
    QCOMPARE(zipFileInfo.zipVersionMadeBy(), quint8(45));
    QCOMPARE(zipFileInfo.zipVersionNeeded(), quint16(20));
    QCOMPARE(zipFileInfo.entryType(), QuaZipFileInfo::File);
    QCOMPARE(zipFileInfo.compressionMethod(), quint16(Z_DEFLATED));
    QCOMPARE(zipFileInfo.compressionLevel(), Z_DEFAULT_COMPRESSION);
    QCOMPARE(zipFileInfo.compressionStrategy(), quint16(Z_DEFAULT_STRATEGY));
    QCOMPARE(zipFileInfo.isText(), isText);
    QCOMPARE(zipFileInfo.isEncrypted(), !password.isEmpty());
    QCOMPARE(quint16(zipFileInfo.externalAttributes()),
        quint16(zipFileInfo.attributes()));
    QCOMPARE(zipFileInfo.filePath(), fileName);
    QCOMPARE(zipFileInfo.fileName(), fileName);
    QCOMPARE(zipFileInfo.path(), QFileInfo(fileName).path());

    QVERIFY(zipFileInfo.isFile());
    QVERIFY(!zipFileInfo.isRaw());
    QVERIFY(!zipFileInfo.isDir());
    QVERIFY(!zipFileInfo.isSymLink());
    QVERIFY(zipFileInfo.symLinkTarget().isEmpty());
    QVERIFY(!zipFileInfo.isHidden());
    QVERIFY(zipFileInfo.isWritable());
    QVERIFY(!zipFileInfo.isExecutable());
    QVERIFY(!zipFileInfo.isSystem());
    QVERIFY(zipFileInfo.isArchived());
    QVERIFY(zipFileInfo.comment().isEmpty());

    QFileInfo fileInfo(dir.filePath(fileName));
    quint32 expectedCrc;
    {
        QFile file(fileInfo.filePath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        expectedCrc = zChecksum<QuaCrc32>(&file);
        QCOMPARE(zipFileInfo.uncompressedSize(), file.size());
        QVERIFY(zipFileInfo.compressedSize() > 0);
    }
    QCOMPARE(zipFileInfo.crc(), expectedCrc);
    QCOMPARE(zipFileInfo.permissions(), fileInfo.permissions());
    QCOMPARE(zipFileInfo.modificationTime(), fileInfo.lastModified());
    QCOMPARE(zipFileInfo.creationTime(), fileInfo.created());
    QCOMPARE(zipFileInfo.lastAccessTime(), fileInfo.lastRead());

    QuaZipFileInfo::ZipSystem expectedSystem;
    if (compatibility & QuaZip::UnixCompatible) {
        expectedSystem = QuaZipFileInfo::OS_UNIX;
    } else if (compatibility == QuaZip::CustomCompatibility ||
        0 != (compatibility & QuaZip::DosCompatible)) {
        expectedSystem = QuaZipFileInfo::OS_MSDOS;
    } else {
        expectedSystem = QuaZipFileInfo::OS_WINDOWS_NTFS;
    }
    QCOMPARE(zipFileInfo.systemMadeBy(), expectedSystem);

    auto expectedMadeBy = quint16(45 | (expectedSystem << 8));
    QCOMPARE(zipFileInfo.madeBy(), expectedMadeBy);

    auto expectedZipOptions =
        QuaZipFileInfo::ZipOptions(QuaZipFileInfo::NormalCompression);
    if (!password.isNull())
        expectedZipOptions |= QuaZipFileInfo::Encryption;
    bool isUnicode = !QuaZUtils::isAscii(fileName);
    if (isUnicode)
        expectedZipOptions |= QuaZipFileInfo::Unicode;
    QCOMPARE(zipFileInfo.zipOptions(), expectedZipOptions);

    quint16 expectedInternalAttributes = 0;
    if (isText)
        expectedInternalAttributes |= QuaZipFileInfo::Text;
    QCOMPARE(zipFileInfo.internalAttributes(), expectedInternalAttributes);

    auto &centralExtra = zipFileInfo.centralExtraFields();
    auto &localExtra = zipFileInfo.localExtraFields();

    bool dosCompatible = 0 != (compatibility & QuaZip::DosCompatible);
    bool unixCompatible = 0 != (compatibility & QuaZip::UnixCompatible);
    bool windowsCompatible = 0 != (compatibility & QuaZip::WindowsCompatible);

    bool hasUnicodePathExtra =
        isUnicode && dosCompatible && (unixCompatible || windowsCompatible);

    QCOMPARE(
        localExtra.contains(INFO_ZIP_UNICODE_PATH_HEADER), hasUnicodePathExtra);

    bool hasZipArchiveExtra = (isUnicode && windowsCompatible) ||
        compatibility == QuaZip::CustomCompatibility;

    QCOMPARE(centralExtra.contains(ZIPARCHIVE_EXTRA_FIELD_HEADER),
        hasZipArchiveExtra);

    QVERIFY(!localExtra.contains(INFO_ZIP_UNICODE_COMMENT_HEADER));

    QCOMPARE(
        centralExtra.contains(UNIX_EXTENDED_TIMESTAMP_HEADER), unixCompatible);

    QCOMPARE(localExtra.contains(UNIX_HEADER), unixCompatible);

    QCOMPARE(
        localExtra.contains(UNIX_EXTENDED_TIMESTAMP_HEADER), unixCompatible);

    QCOMPARE(localExtra.contains(NTFS_HEADER),
        compatibility != QuaZip::DosCompatible);

    if (!password.isNull()) {
        QuaZipKeysGenerator keyGen;
        keyGen.addPassword(password);
        QVERIFY(memcmp(keyGen.keys(), zipFileInfo.cryptKeys(),
                    sizeof(QuaZipKeysGenerator::Keys)) == 0);
    }
}
