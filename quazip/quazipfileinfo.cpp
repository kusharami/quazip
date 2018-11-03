/*
Copyright (C) 2005-2014 Sergey A. Tachenov
Copyright (C) 2018 Alexandra Cherdantseva

This file is part of QuaZIP.

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

#include "quazipfileinfo.h"

#include <QDataStream>
#include <QDir>

#include <zlib.h>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

enum
{
    UNX_IFMT = 0170000,
    UNX_IFREG = 0100000,
    UNX_IFLNK = 0120000,
    UNX_IFDIR = 0040000,
    UNX_IALL = 00777,
    UNX_IRUSR = 00400, /* Unix read permission: owner */
    UNX_IWUSR = 00200, /* Unix write permission: owner */
    UNX_IXUSR = 00100, /* Unix execute permission: owner */
    UNX_IRGRP = 00040, /* Unix read permission: group */
    UNX_IWGRP = 00020, /* Unix write permission: group */
    UNX_IXGRP = 00010, /* Unix execute permission: group */
    UNX_IROTH = 00004, /* Unix read permission: other */
    UNX_IWOTH = 00002, /* Unix write permission: other */
    UNX_IXOTH = 00001, /* Unix execute permission: other */

    AMI_IFMT = 06000, /* Amiga file type mask */
    AMI_IFDIR = 04000, /* Amiga directory */
    AMI_IFREG = 02000, /* Amiga regular file */
    AMI_IHIDDEN = 00200, /* to be supported in AmigaDOS 3.x */
    AMI_ISCRIPT = 00100, /* executable script (text command file) */
    AMI_IPURE = 00040, /* allow loading into resident memory */
    AMI_IARCHIVE = 00020, /* not modified since bit was last set */
    AMI_IREAD = 00010, /* can be opened for reading */
    AMI_IWRITE = 00004, /* can be opened for writing */
    AMI_IEXECUTE = 00002, /* executable image, a loadable runfile */
    AMI_IDELETE = 00001, /* can be deleted */
    AMI_IALL = AMI_IREAD | AMI_IWRITE | AMI_IEXECUTE | AMI_IDELETE,

    THS_IFMT = 0xF000,
    THS_IFDIR = 0x4000,
    THS_IFREG = 0x8000,
    THS_IMODF = 0x0800, /* modified */
    THS_INHID = 0x0400, /* not hidden */
    THS_IALL = 0x03FF,
    THS_IEUSR = 0x0200, /* erase permission: owner */
    THS_IRUSR = 0x0100, /* read permission: owner */
    THS_IWUSR = 0x0080, /* write permission: owner */
    THS_IXUSR = 0x0040, /* execute permission: owner */
    THS_IROTH = 0x0004, /* read permission: other */
    THS_IWOTH = 0x0002, /* write permission: other */
    THS_IXOTH = 0x0001, /* execute permission: other */

    RAW_FLAG = 1 << 0,
    HAS_KEYS_FLAG = 1 << 1
};

struct QuaZipFileInfo::Private : public QSharedData {
    quint32 crc;
    qint32 externalAttributes;
    quint16 internalAttributes;
    quint16 flags;
    quint8 zipVersionMadeBy;
    ZipSystem zipSystem;
    ZipOptions zipOptions;
    quint16 compressionMethod;
    quint16 compressionStrategy;
    quint16 zipVersionNeeded;
    int diskNumber;
    int compressionLevel;
    qint64 uncompressedSize;
    qint64 compressedSize;
    QDateTime createTime;
    QDateTime modifyTime;
    QDateTime accessTime;
    QString filePath;
    QString comment;
    QString symLinkTarget;
    QuaZExtraField::Map centralExtraFields;
    QuaZExtraField::Map localExtraFields;
    QuaZipKeysGenerator::Keys cryptKeys;

    Private();
    Private(const Private &other);

    void setEntryType(EntryType value);
    void setPermissions(QFile::Permissions value);

    EntryType entryType() const;
    QFile::Permissions permissions() const;
    Attributes attributes() const;
    void adjustFilePath(bool isDir);
    bool equals(const Private &other) const;
};

QuaZipFileInfo::QuaZipFileInfo()
    : d(new Private)
{
}

QuaZipFileInfo::QuaZipFileInfo(const QString &filePath)
    : d(new Private)
{
    setFilePath(filePath);
}

QuaZipFileInfo::QuaZipFileInfo(const QuaZipFileInfo &other)
{
    operator=(other);
}

QuaZipFileInfo::~QuaZipFileInfo()
{
    // here is private data destroyed
}

QuaZipFileInfo QuaZipFileInfo::fromFile(
    const QString &filePath, const QString &storePath)
{
    QuaZipFileInfo info(storePath);
    info.initWithFile(filePath);
    return info;
}

QuaZipFileInfo QuaZipFileInfo::fromFile(
    const QFileInfo &fileInfo, const QString &storePath)
{
    QuaZipFileInfo info(storePath);
    info.initWithFile(fileInfo);
    return info;
}

QuaZipFileInfo QuaZipFileInfo::fromDir(
    const QDir &dir, const QString &storePath)
{
    QuaZipFileInfo info(storePath);
    info.initWithDir(dir);
    return info;
}

bool QuaZipFileInfo::initWithDir(const QDir &dir)
{
    auto path = dir.absolutePath();
    if (path.isEmpty() || path.endsWith('/'))
        return false;

    return initWithFile(path);
}

bool QuaZipFileInfo::initWithFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    return initWithFile(fileInfo);
}

bool QuaZipFileInfo::initWithFile(const QFileInfo &fileInfo)
{
    if ((!fileInfo.exists() && !fileInfo.isSymLink()))
        return false;

    if (!fileInfo.isSymLink() && fileInfo.fileName().isEmpty()) {
        QDir dir(fileInfo.filePath());
        return initWithDir(dir);
    }

    if (d.constData()->filePath.isEmpty())
        setFilePath(fileInfo.fileName());
    if (fileInfo.isSymLink()) {
        d->setEntryType(SymLink);
        d->symLinkTarget = fileInfo.symLinkTarget();
    } else if (fileInfo.isDir()) {
        d->setEntryType(Directory);
    } else {
        d->setEntryType(File);
    }

    Attributes attr;
    auto perm = fileInfo.permissions();
    if (!fileInfo.isWritable())
        attr |= ReadOnly;

    if (fileInfo.isHidden())
        attr |= Hidden;

    if (fileInfo.isExecutable())
        perm |= QFile::ExeUser | QFile::ExeOwner;

    setAttributes(attr);
    setPermissions(perm);

    setModificationTime(fileInfo.lastModified());
    setLastAccessTime(fileInfo.lastRead());
    setCreationTime(fileInfo.created());
    setUncompressedSize(fileInfo.size());

    return true;
}

bool QuaZipFileInfo::applyAttributesTo(const QString &filePath,
    Attributes attributes, QFile::Permissions permissions)
{
    if (filePath.isEmpty())
        return false;

    auto nativeFilePath =
        QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath());
    if (!QFile::exists(filePath))
        return false;
    if (permissions == 0) {
        permissions = QFile::permissions(filePath);
    }

    if (attributes & ReadOnly)
        permissions &= ~(QFile::WriteOwner | QFile::WriteUser |
            QFile::WriteGroup | QFile::WriteOther);
    else
        permissions |= QFile::WriteOwner | QFile::WriteUser;

    bool ok = QFile::setPermissions(filePath, permissions);

#ifdef Q_OS_WIN
    auto attr = attributes & ~DirAttr;

    static_assert(sizeof(WCHAR) == sizeof(decltype(*nativeFilePath.utf16())),
        "WCHAR size mismatch");
    ok = SetFileAttributesW(
             reinterpret_cast<const WCHAR *>(nativeFilePath.utf16()),
             DWORD(attr)) &&
        ok;
#endif
    return ok;
}

bool QuaZipFileInfo::applyAttributesTo(const QString &filePath) const
{
    return applyAttributesTo(filePath, attributes(), permissions());
}

QuaZipFileInfo::EntryType QuaZipFileInfo::entryType() const
{
    return d->entryType();
}

QuaZipFileInfo::EntryType QuaZipFileInfo::Private::entryType() const
{
    if (filePath.endsWith('/'))
        return Directory;

    int uAttr = externalAttributes >> 16;

    switch (zipSystem) {
    case OS_MSDOS:
    case OS_WINDOWS_NTFS:
    case OS_WINDOWS_VFAT:
    case OS_OS2HPFS:
    case OS_MVS:
    case OS_VMCMS:
    case OS_ACORN:
        if (uAttr & QuaZipFileInfo::DirAttr)
            return QuaZipFileInfo::Directory;
        break;

    case OS_AMIGA:
        if (AMI_IFDIR == (uAttr & AMI_IFMT)) {
            return QuaZipFileInfo::Directory;
        }
        break;

    case OS_THEOS:
        if (THS_IFDIR == (uAttr & THS_IFMT)) {
            return QuaZipFileInfo::Directory;
        }
        break;

    case OS_QDOS:
    case OS_OPENVMS:
    case OS_ZSYSTEM:
    case OS_CPM:
    case OS_TANDEM:
    case OS_ATARI:
    case OS_BEOS:
    case OS_TOPS20:
    case OS_MACINTOSH:
    case OS_MACOSX:
    case OS_UNIX:
        switch (uAttr & UNX_IFMT) {
        case UNX_IFDIR:
            return Directory;

        case UNX_IFLNK:
            if (isSymLinkHost(zipSystem))
                return SymLink;
            break;
        }
        break;
    }

    return File;
}

void QuaZipFileInfo::setEntryType(EntryType value)
{
    if (entryType() == value)
        return;

    d->setEntryType(value);
}

void QuaZipFileInfo::Private::setEntryType(EntryType value)
{
    int uAttr = externalAttributes >> 16;

    switch (value) {
    case File:
        externalAttributes &= ~DirAttr;
        switch (zipSystem) {
        case OS_VMCMS:
        case OS_ACORN:
        case OS_MSDOS:
        case OS_WINDOWS_NTFS:
        case OS_WINDOWS_VFAT:
        case OS_OS2HPFS:
        case OS_MVS:
            break;

        case OS_QDOS:
        case OS_OPENVMS:
        case OS_ZSYSTEM:
        case OS_CPM:
        case OS_TANDEM:
        case OS_ATARI:
        case OS_BEOS:
        case OS_TOPS20:
        case OS_MACINTOSH:
        case OS_UNIX:
        case OS_MACOSX:
            uAttr &= ~UNX_IFMT;
            uAttr |= UNX_IFREG;
            break;

        case OS_AMIGA:
            uAttr &= ~AMI_IFMT;
            uAttr |= AMI_IFREG;
            break;

        case OS_THEOS:
            uAttr &= ~THS_IFMT;
            uAttr |= THS_IFREG;
            break;
        }
        break;

    case Directory:
        externalAttributes |= DirAttr;
        switch (zipSystem) {
        case OS_MSDOS:
        case OS_WINDOWS_NTFS:
        case OS_WINDOWS_VFAT:
        case OS_OS2HPFS:
        case OS_MVS:
        case OS_VMCMS:
        case OS_ACORN:
            break;

        case OS_QDOS:
        case OS_OPENVMS:
        case OS_ZSYSTEM:
        case OS_CPM:
        case OS_TANDEM:
        case OS_ATARI:
        case OS_BEOS:
        case OS_TOPS20:
        case OS_MACINTOSH:
        case OS_UNIX:
        case OS_MACOSX:
            uAttr &= ~UNX_IFMT;
            uAttr |= UNX_IFDIR;
            break;

        case OS_AMIGA:
            uAttr &= ~AMI_IFMT;
            uAttr |= AMI_IFDIR;
            break;

        case OS_THEOS:
            uAttr &= ~THS_IFMT;
            uAttr |= THS_IFDIR;
            break;
        }
        break;

    case SymLink: {
        auto perm = permissions();
        zipSystem = OS_UNIX;
        setPermissions(perm);
        uAttr = externalAttributes >> 16;

        externalAttributes &= ~DirAttr;

        uAttr &= UNX_IFMT;
        uAttr |= UNX_IFLNK;
        break;
    }
    }

    externalAttributes &= 0xFFFF;
    externalAttributes |= uAttr << 16;
    adjustFilePath(value == Directory);
}

const QString &QuaZipFileInfo::filePath() const
{
    return d->filePath;
}

void QuaZipFileInfo::setFilePath(const QString &filePath)
{
    auto normalizedFilePath = QDir::cleanPath(filePath);
    if (normalizedFilePath.startsWith('/')) {
        normalizedFilePath = normalizedFilePath.mid(1);
    }

    if (d.constData()->filePath == normalizedFilePath)
        return;

    d->filePath = normalizedFilePath;
    auto attr = d.constData()->attributes();
    attr.setFlag(DirAttr, filePath.endsWith('/') || filePath.endsWith('\\'));
    setAttributes(attr);
}

QString QuaZipFileInfo::fileName() const
{
    QFileInfo fileInfo(filePath());

    if (fileInfo.fileName().isEmpty())
        fileInfo.setFile(fileInfo.path());

    return fileInfo.fileName();
}

QString QuaZipFileInfo::path() const
{
    QFileInfo fileInfo(filePath());

    if (fileInfo.fileName().isEmpty())
        fileInfo.setFile(fileInfo.path());

    return fileInfo.path();
}

const QDateTime &QuaZipFileInfo::creationTime() const
{
    return d->createTime;
}

void QuaZipFileInfo::setCreationTime(const QDateTime &time)
{
    if (creationTime() == time)
        return;

    d->createTime = time;
}

const QDateTime &QuaZipFileInfo::modificationTime() const
{
    return d->modifyTime;
}

void QuaZipFileInfo::setModificationTime(const QDateTime &time)
{
    if (modificationTime() == time)
        return;

    d->modifyTime = time;
}

const QDateTime &QuaZipFileInfo::lastAccessTime() const
{
    return d->accessTime;
}

void QuaZipFileInfo::setLastAccessTime(const QDateTime &time)
{
    if (lastAccessTime() == time)
        return;

    d->accessTime = time;
}

qint64 QuaZipFileInfo::uncompressedSize() const
{
    return d->uncompressedSize;
}

void QuaZipFileInfo::setUncompressedSize(qint64 size)
{
    if (uncompressedSize() == size)
        return;

    d->uncompressedSize = size;
}

qint64 QuaZipFileInfo::compressedSize() const
{
    return d->compressedSize;
}

void QuaZipFileInfo::setCompressedSize(qint64 size)
{
    if (compressedSize() == size)
        return;

    d->compressedSize = size;
}

quint32 QuaZipFileInfo::crc() const
{
    return d->crc;
}

void QuaZipFileInfo::setCrc(quint32 value)
{
    if (crc() == value)
        return;

    d->crc = value;
}

const QString &QuaZipFileInfo::comment() const
{
    return d->comment;
}

void QuaZipFileInfo::setComment(const QString &value)
{
    d->comment = value;
}

void QuaZipFileInfo::setPassword(QByteArray *value)
{
    if (value == nullptr || value->isNull()) {
        setZipOptions(d.constData()->zipOptions & ~Encryption);
        clearCryptKeys();
        return;
    }

    QuaZipKeysGenerator keyGen;
    keyGen.addPassword(*value);
    setCryptKeys(keyGen.keys());
}

quint16 QuaZipFileInfo::madeBy() const
{
    return quint16((quint8(d->zipSystem) << 8) | d->zipVersionMadeBy);
}

void QuaZipFileInfo::setMadeBy(quint16 value)
{
    if (madeBy() == value)
        return;

    d->zipVersionMadeBy = quint8(value);
    d->zipSystem = ZipSystem(value >> 8);
}

quint16 QuaZipFileInfo::zipVersionNeeded() const
{
    return d->zipVersionNeeded;
}

void QuaZipFileInfo::setZipVersionNeeded(quint16 value)
{
    if (zipVersionNeeded() == value)
        return;

    d->zipVersionNeeded = value;
}

QuaZipFileInfo::ZipSystem QuaZipFileInfo::systemMadeBy() const
{
    return d->zipSystem;
}

void QuaZipFileInfo::setSystemMadeBy(ZipSystem value)
{
    if (systemMadeBy() == value)
        return;

    d->zipSystem = value;
}

quint8 QuaZipFileInfo::zipVersionMadeBy() const
{
    return d->zipVersionMadeBy;
}

void QuaZipFileInfo::setZipVersionMadeBy(quint8 spec)
{
    if (zipVersionMadeBy() == spec)
        return;

    d->zipVersionMadeBy = spec;
}

quint16 QuaZipFileInfo::internalAttributes()
{
    return d->internalAttributes;
}

void QuaZipFileInfo::setInternalAttributes(quint16 value)
{
    if (internalAttributes() == value)
        return;

    d->internalAttributes = value;
}

qint32 QuaZipFileInfo::externalAttributes()
{
    return d->externalAttributes;
}

void QuaZipFileInfo::setExternalAttributes(qint32 value)
{
    if (externalAttributes() == value)
        return;

    d->externalAttributes = value;
    d->adjustFilePath(d.constData()->attributes() & DirAttr);
}

const QString &QuaZipFileInfo::symLinkTarget() const
{
    return d->symLinkTarget;
}

void QuaZipFileInfo::setSymLinkTarget(const QString &filePath)
{
    if (entryType() == SymLink && symLinkTarget() == filePath)
        return;

    d->symLinkTarget = filePath;
    setEntryType(SymLink);
}

bool QuaZipFileInfo::isEncrypted() const
{
    return d->zipOptions.testFlag(Encryption);
}

void QuaZipFileInfo::setIsEncrypted(bool value)
{
    if (isEncrypted() == value)
        return;

    d->zipOptions.setFlag(Encryption, value);
}

const QuaZipKeysGenerator::Keys &QuaZipFileInfo::cryptKeys() const
{
    return d->cryptKeys;
}

void QuaZipFileInfo::setCryptKeys(const QuaZipKeysGenerator::Keys &keys)
{
    if (hasCryptKeys() && isEncrypted() &&
        0 == memcmp(keys, cryptKeys(), sizeof(QuaZipKeysGenerator::Keys))) {
        return;
    }

    memcpy(d->cryptKeys, keys, sizeof(QuaZipKeysGenerator::Keys));
    d->zipOptions |= Encryption;
    d->flags |= HAS_KEYS_FLAG;
}

bool QuaZipFileInfo::hasCryptKeys() const
{
    return 0 != (d->flags & HAS_KEYS_FLAG);
}

void QuaZipFileInfo::clearCryptKeys()
{
    if (0 == (d.constData()->flags & HAS_KEYS_FLAG))
        return;

    memset(d->cryptKeys, 0, sizeof(QuaZipKeysGenerator::Keys));
    d->flags &= ~HAS_KEYS_FLAG;
}

quint16 QuaZipFileInfo::compressionMethod() const
{
    return d->compressionMethod;
}

void QuaZipFileInfo::setCompressionMethod(quint16 method)
{
    if (compressionMethod() == method)
        return;

    d->compressionMethod = method;
}

quint16 QuaZipFileInfo::compressionStrategy() const
{
    return d->compressionStrategy;
}

void QuaZipFileInfo::setCompressionStrategy(quint16 value)
{
    if (compressionStrategy() == value)
        return;

    d->compressionStrategy = value;
}

int QuaZipFileInfo::compressionLevel() const
{
    return d->compressionLevel;
}

void QuaZipFileInfo::setCompressionLevel(int level)
{
    auto originalMethod = d.constData()->compressionMethod;
    auto originalOptions = d.constData()->zipOptions;

    auto method = originalMethod;
    auto zipOptions = originalOptions;

    zipOptions &= ~CompressionFlags;
    if (method == Z_NO_COMPRESSION || method == Z_DEFLATED) {
        switch (level) {
        case Z_NO_COMPRESSION:
            method = Z_NO_COMPRESSION;
            break;

        case Z_BEST_SPEED:
            method = Z_DEFLATED;
            zipOptions |= SuperFastCompression;
            break;

        default:
            method = Z_DEFLATED;
            if (level >= Z_BEST_COMPRESSION) {
                zipOptions |= MaximumCompression;
            } else if (level > 0 && level < 5) {
                zipOptions |= FastCompression;
            }
            break;
        }
    }

    if (method == originalMethod && zipOptions == originalOptions &&
        compressionLevel() == level) {
        return;
    }

    d->compressionLevel = level;
    d->compressionMethod = method;
    d->zipOptions = zipOptions;
}

int QuaZipFileInfo::detectCompressionLevel() const
{
    switch (d->compressionMethod) {
    case Z_DEFLATED:
        switch (d->zipOptions & CompressionFlags) {
        case NormalCompression:
            return Z_DEFAULT_COMPRESSION;

        case MaximumCompression:
            return Z_BEST_COMPRESSION;

        case FastCompression:
            return Z_BEST_SPEED + 2;

        case SuperFastCompression:
            return Z_BEST_SPEED;
        }
        break;

    case Z_NO_COMPRESSION:
        return Z_NO_COMPRESSION;
    }

    return Z_DEFAULT_COMPRESSION;
}

QuaZipFileInfo::ZipOptions QuaZipFileInfo::zipOptions() const
{
    return d->zipOptions;
}

void QuaZipFileInfo::setZipOptions(ZipOptions options)
{
    if (zipOptions() == options)
        return;

    d->zipOptions = options;
}

bool QuaZipFileInfo::isRaw() const
{
    return 0 != (d->flags & RAW_FLAG);
}

void QuaZipFileInfo::setIsRaw(bool value)
{
    if (isRaw() == value)
        return;

    if (value)
        d->flags |= RAW_FLAG;
    else
        d->flags &= ~RAW_FLAG;
}

bool QuaZipFileInfo::isText() const
{
    return 0 != (d->internalAttributes & Text);
}

void QuaZipFileInfo::setIsText(bool value)
{
    if (isText() == value)
        return;

    if (value)
        d->internalAttributes |= Text;
    else
        d->internalAttributes &= ~Text;
}

const QuaZExtraField::Map &QuaZipFileInfo::centralExtraFields() const
{
    return d->centralExtraFields;
}

void QuaZipFileInfo::setCentralExtraFields(const QuaZExtraField::Map &map)
{
    d->centralExtraFields = map;
}

const QuaZExtraField::Map &QuaZipFileInfo::localExtraFields() const
{
    return d->localExtraFields;
}

void QuaZipFileInfo::setLocalExtraFields(const QuaZExtraField::Map &map)
{
    d->localExtraFields = map;
}

QFile::Permissions QuaZipFileInfo::permissions() const
{
    return d->permissions();
}

QFile::Permissions QuaZipFileInfo::Private::permissions() const
{
    QFile::Permissions permissions;
    int uAttr = externalAttributes >> 16;

    switch (zipSystem) {
    case OS_MSDOS:
    case OS_WINDOWS_NTFS:
    case OS_WINDOWS_VFAT:
    case OS_OS2HPFS:
    case OS_MVS:
    case OS_VMCMS:
    case OS_ACORN:
        permissions = QFile::ReadUser | QFile::ReadOwner | QFile::ReadGroup |
            QFile::ReadOther;
        if (0 == (externalAttributes & QuaZipFileInfo::ReadOnly)) {
            permissions |= QFile::WriteOwner | QFile::WriteUser |
                QFile::WriteOther | QFile::WriteGroup;
        }
        break;

    case OS_AMIGA:
        if (uAttr & AMI_IREAD)
            permissions |= QFile::ReadUser | QFile::ReadOwner |
                QFile::ReadOther | QFile::ReadGroup;

        if (uAttr & (AMI_IWRITE | AMI_IDELETE))
            permissions |= QFile::WriteUser | QFile::WriteOwner |
                QFile::WriteGroup | QFile::WriteOther;

        if (uAttr & AMI_IEXECUTE)
            permissions |= QFile::ExeUser | QFile::ExeOwner;

        break;

    case OS_THEOS:
        if (uAttr & THS_IRUSR) {
            permissions |=
                QFile::ReadUser | QFile::ReadOwner | QFile::ReadGroup;
        }

        if (uAttr & (THS_IEUSR | THS_IWUSR))
            permissions |= QFile::WriteUser | QFile::WriteOwner;

        if (uAttr & THS_IXUSR)
            permissions |= QFile::ExeUser | QFile::ExeOwner;

        if (uAttr & THS_IROTH)
            permissions |= QFile::ReadOther;

        if (uAttr & THS_IWOTH)
            permissions |= QFile::WriteOther;

        if (uAttr & THS_IXOTH)
            permissions |= QFile::ExeOther;

        break;

    case OS_QDOS:
    case OS_OPENVMS:
    case OS_ZSYSTEM:
    case OS_CPM:
    case OS_TANDEM:
    case OS_ATARI:
    case OS_BEOS:
    case OS_TOPS20:
    case OS_MACINTOSH:
    case OS_MACOSX:
    case OS_UNIX:
        if (uAttr & UNX_IRUSR)
            permissions |= QFile::ReadUser | QFile::ReadOwner;

        if (uAttr & UNX_IWUSR)
            permissions |= QFile::WriteUser | QFile::WriteOwner;

        if (uAttr & UNX_IXUSR)
            permissions |= QFile::ExeUser | QFile::ExeOwner;

        if (uAttr & UNX_IRGRP)
            permissions |= QFile::ReadGroup;

        if (uAttr & UNX_IWGRP)
            permissions |= QFile::WriteGroup;

        if (uAttr & UNX_IXGRP)
            permissions |= QFile::ExeGroup;

        if (uAttr & UNX_IROTH)
            permissions |= QFile::ReadOther;

        if (uAttr & UNX_IWOTH)
            permissions |= QFile::WriteOther;

        if (uAttr & UNX_IXOTH)
            permissions |= QFile::ExeOther;

        break;
    }

    if (permissions == 0) {
        permissions = QFile::ReadOwner | QFile::ReadUser;
    }

    return permissions;
}

void QuaZipFileInfo::setPermissions(QFile::Permissions value)
{
    if (permissions() == value)
        return;

    d->setPermissions(value);
}

void QuaZipFileInfo::Private::setPermissions(QFile::Permissions value)
{
    int uAttr = externalAttributes >> 16;

    auto testPerm = value;
    if (testPerm & (QFile::ReadUser | QFile::ReadOwner)) {
        testPerm &= ~QFile::ReadOwner;
        testPerm |= QFile::ReadUser;
    }

    if (testPerm & (QFile::WriteUser | QFile::WriteOwner)) {
        testPerm &= ~QFile::WriteOwner;
        testPerm |= QFile::WriteUser;
    }

    Q_CONSTEXPR auto allRead =
        QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther;
    Q_CONSTEXPR auto allWrite =
        QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther;

    if (testPerm & allWrite) {
        externalAttributes &= ~ReadOnly;
    } else {
        externalAttributes |= ReadOnly;
    }

    switch (zipSystem) {
    case OS_MSDOS:
    case OS_WINDOWS_NTFS:
    case OS_WINDOWS_VFAT:
    case OS_OS2HPFS:
    case OS_MVS:
    case OS_VMCMS:
    case OS_ACORN: {
        if (testPerm == allRead || testPerm == (allRead | allWrite)) {
            break;
        }

        uAttr &= ~UNX_IFMT;
        switch (entryType()) {
        case Directory:
            uAttr |= UNX_IFDIR;
            break;

        case File:
            uAttr |= UNX_IFREG;
            break;

        case SymLink:
            uAttr |= UNX_IFLNK;
            break;
        }
        zipSystem = OS_UNIX;
        Q_FALLTHROUGH();
    }
    case OS_QDOS:
    case OS_OPENVMS:
    case OS_ZSYSTEM:
    case OS_CPM:
    case OS_TANDEM:
    case OS_ATARI:
    case OS_BEOS:
    case OS_TOPS20:
    case OS_MACINTOSH:
    case OS_MACOSX:
    case OS_UNIX:
        uAttr &= ~UNX_IALL;
        if (value & (QFile::ReadUser | QFile::ReadOwner))
            uAttr |= UNX_IRUSR;

        if (value & (QFile::WriteUser | QFile::WriteOwner))
            uAttr |= UNX_IWUSR;

        if (value & (QFile::ExeUser | QFile::ExeOwner))
            uAttr |= UNX_IXUSR;

        if (value & QFile::ReadGroup)
            uAttr |= UNX_IRGRP;

        if (value & QFile::WriteGroup)
            uAttr |= UNX_IWGRP;

        if (value & QFile::ExeGroup)
            uAttr |= UNX_IXGRP;

        if (value & QFile::ReadOther)
            uAttr |= UNX_IROTH;

        if (value & QFile::WriteOther)
            uAttr |= UNX_IWOTH;

        if (value & QFile::ExeOther)
            uAttr |= UNX_IXOTH;

        break;

    case OS_AMIGA:
        uAttr &= ~AMI_IALL;
        if (testPerm & allRead)
            uAttr |= AMI_IREAD;

        if (testPerm & allWrite)
            uAttr |= AMI_IWRITE | AMI_IDELETE;

        if (value & (QFile::ExeOwner | QFile::ExeUser))
            uAttr |= AMI_IEXECUTE;

        break;

    case OS_THEOS:
        uAttr &= ~THS_IALL;
        if (value & (QFile::ReadUser | QFile::ReadOwner | QFile::ReadGroup))
            uAttr |= THS_IRUSR;

        if (value & (QFile::WriteUser | QFile::WriteOwner | QFile::WriteGroup))
            uAttr |= THS_IWUSR | THS_IEUSR;

        if (value & (QFile::ExeUser | QFile::ExeOwner))
            uAttr |= THS_IXUSR;

        if (value & QFile::ReadOther)
            uAttr |= THS_IROTH;

        if (value & QFile::WriteOther)
            uAttr |= THS_IWOTH;

        break;
    }

    externalAttributes &= 0xFFFF;
    externalAttributes |= uAttr << 16;
}

QuaZipFileInfo::Attributes QuaZipFileInfo::attributes() const
{
    auto result = d->attributes() & AllAttrs;

    if (d->filePath.endsWith('/'))
        result |= DirAttr;

    return result;
}

QuaZipFileInfo::Attributes QuaZipFileInfo::Private::attributes() const
{
    Attributes result(externalAttributes & 0xFF);
    int uAttr = externalAttributes >> 16;

    switch (zipSystem) {
    case OS_MSDOS:
    case OS_WINDOWS_NTFS:
    case OS_WINDOWS_VFAT:
    case OS_OS2HPFS:
    case OS_MVS:
    case OS_VMCMS:
    case OS_ACORN:
        break;

    case OS_AMIGA:
        result.setFlag(DirAttr, AMI_IFDIR == (uAttr & AMI_IFMT));
        result.setFlag(Hidden, uAttr & AMI_IHIDDEN);
        result.setFlag(Archived, uAttr & AMI_IARCHIVE);
        break;

    case OS_THEOS:
        result.setFlag(DirAttr, THS_IFDIR == (uAttr & THS_IFMT));
        result.setFlag(Hidden, !(uAttr & THS_INHID));
        result.setFlag(Archived, !(uAttr & THS_IMODF));
        break;

    case OS_MACOSX:
    case OS_UNIX:
    case OS_QDOS:
    case OS_OPENVMS:
    case OS_ZSYSTEM:
    case OS_CPM:
    case OS_TANDEM:
    case OS_ATARI:
    case OS_BEOS:
    case OS_TOPS20:
    case OS_MACINTOSH:
        result.setFlag(Hidden, QFileInfo(filePath).fileName().startsWith('.'));
        result.setFlag(DirAttr, UNX_IFDIR == (uAttr & UNX_IFMT));
        result |= Archived;
        break;
    }

    result.setFlag(ReadOnly,
        !(permissions() &
            (QFile::WriteGroup | QFile::WriteOwner | QFile::WriteUser |
                QFile::WriteOther)));

    return result;
}

void QuaZipFileInfo::setAttributes(Attributes value)
{
    value &= AllAttrs;
    if (attributes() == value)
        return;

    auto tempFilePath = d->filePath;
    if (tempFilePath.endsWith('/'))
        tempFilePath.chop(1);

    int uAttr = d->externalAttributes >> 16;

    switch (d->zipSystem) {
    case OS_MSDOS:
    case OS_WINDOWS_NTFS:
    case OS_WINDOWS_VFAT:
    case OS_OS2HPFS:
    case OS_MVS:
    case OS_VMCMS:
    case OS_ACORN:
        break;

    case OS_AMIGA:
        uAttr &= ~AMI_IFMT;
        if (value & DirAttr)
            uAttr |= AMI_IFDIR;
        else
            uAttr |= AMI_IFREG;

        if (value & Archived)
            uAttr |= AMI_IARCHIVE;
        else
            uAttr &= ~AMI_IARCHIVE;

        if (value & Hidden)
            uAttr |= AMI_IHIDDEN;
        else
            uAttr &= ~AMI_IHIDDEN;
        break;

    case OS_THEOS:
        uAttr &= ~THS_IFMT;
        if (value & DirAttr)
            uAttr |= THS_IFDIR;
        else
            uAttr |= THS_IFREG;

        if (value & Archived)
            uAttr &= ~THS_IMODF;
        else
            uAttr |= THS_IMODF;

        if (value & Hidden)
            uAttr &= ~THS_INHID;
        else
            uAttr |= THS_INHID;

        break;

    case OS_QDOS:
    case OS_OPENVMS:
    case OS_ZSYSTEM:
    case OS_CPM:
    case OS_TANDEM:
    case OS_ATARI:
    case OS_BEOS:
    case OS_TOPS20:
    case OS_MACINTOSH:
    case OS_MACOSX:
    case OS_UNIX: {
        bool wasSymLink = UNX_IFLNK == (uAttr & UNX_IFMT);
        uAttr &= ~UNX_IFMT;

        if (tempFilePath.isEmpty())
            break;

        QFileInfo fileInfo(tempFilePath);
        if (value & DirAttr) {
            uAttr |= UNX_IFDIR;
        } else {
            uAttr |= wasSymLink ? UNX_IFLNK : UNX_IFREG;
        }

        auto fileName = fileInfo.fileName();
        if (value & Hidden) {
            if (!fileName.startsWith('.')) {
                fileInfo.setFile(fileInfo.dir().filePath('.' + fileName));
            }
        } else {
            if (fileName.startsWith('.')) {
                fileInfo.setFile(fileInfo.dir().filePath(fileName.mid(1)));
            }
        }

        tempFilePath = fileInfo.filePath();
        break;
    }
    }

    if (!tempFilePath.isEmpty() && 0 != (value & DirAttr)) {
        tempFilePath += '/';
    }
    d->filePath = tempFilePath;
    d->externalAttributes &= (0xFFFF | ~AllAttrs);
    d->externalAttributes |= value | (uAttr << 16);
}

int QuaZipFileInfo::diskNumber() const
{
    return d->diskNumber;
}

void QuaZipFileInfo::setDiskNumber(int value)
{
    d->diskNumber = value;
}

QuaZipFileInfo &QuaZipFileInfo::operator=(const QuaZipFileInfo &other)
{
    d = other.d;
    return *this;
}

bool QuaZipFileInfo::operator==(const QuaZipFileInfo &other) const
{
    return d == other.d || d->equals(*other.d.data());
}

bool QuaZipFileInfo::isUnixHost(ZipSystem host)
{
    switch (host) {
    case OS_QDOS:
    case OS_OPENVMS:
    case OS_ZSYSTEM:
    case OS_CPM:
    case OS_TANDEM:
    case OS_ATARI:
    case OS_BEOS:
    case OS_TOPS20:
    case OS_MACINTOSH:
    case OS_UNIX:
    case OS_MACOSX:
        return true;

    case OS_MSDOS:
    case OS_WINDOWS_NTFS:
    case OS_WINDOWS_VFAT:
    case OS_OS2HPFS:
    case OS_MVS:
    case OS_VMCMS:
    case OS_ACORN:
    case OS_THEOS:
    case OS_AMIGA:
        break;
    }

    return false;
}

bool QuaZipFileInfo::isSymLinkHost(ZipSystem host)
{
    switch (host) {
    case OS_UNIX:
    case OS_MACOSX:
    case OS_BEOS:
    case OS_OPENVMS:
    case OS_ATARI:
        return true;

    case OS_QDOS:
    case OS_ZSYSTEM:
    case OS_CPM:
    case OS_TANDEM:
    case OS_TOPS20:
    case OS_MACINTOSH:
    case OS_MSDOS:
    case OS_WINDOWS_NTFS:
    case OS_WINDOWS_VFAT:
    case OS_OS2HPFS:
    case OS_MVS:
    case OS_VMCMS:
    case OS_ACORN:
    case OS_THEOS:
    case OS_AMIGA:
        break;
    }

    return false;
}

QuaZipFileInfo::Private::Private()
    : crc(0)
    , externalAttributes(0)
    , internalAttributes(0)
    , flags(0)
    , zipVersionMadeBy(10)
    , zipSystem(OS_MSDOS)
    , zipOptions(NormalCompression)
    , compressionMethod(Z_DEFLATED)
    , compressionStrategy(Z_DEFAULT_STRATEGY)
    , zipVersionNeeded(10)
    , diskNumber(0)
    , compressionLevel(Z_DEFAULT_COMPRESSION)
    , uncompressedSize(0)
    , compressedSize(0)
{
    memset(cryptKeys, 0, sizeof(cryptKeys));
}

QuaZipFileInfo::Private::Private(const Private &other)
    : crc(other.crc) //0
    , externalAttributes(other.externalAttributes) //1
    , internalAttributes(other.internalAttributes) //2
    , flags(other.flags) //3
    , zipVersionMadeBy(other.zipVersionMadeBy) //4
    , zipSystem(other.zipSystem) //5
    , zipOptions(other.zipOptions) //6
    , compressionMethod(other.compressionMethod) //7
    , compressionStrategy(other.compressionStrategy) //8
    , zipVersionNeeded(other.zipVersionNeeded) //9
    , diskNumber(other.diskNumber) //10
    , compressionLevel(other.compressionLevel) //22
    , uncompressedSize(other.uncompressedSize) //11
    , compressedSize(other.compressedSize) //12
    , createTime(other.createTime) //13
    , modifyTime(other.modifyTime) //14
    , accessTime(other.accessTime) //15
    , filePath(other.filePath) //16
    , comment(other.comment) //17
    , symLinkTarget(other.symLinkTarget) //21
    , centralExtraFields(other.centralExtraFields) //18
    , localExtraFields(other.localExtraFields) //19
{
    memcpy(cryptKeys, other.cryptKeys, sizeof(cryptKeys)); //20
}

void QuaZipFileInfo::Private::adjustFilePath(bool isDir)
{
    if (isDir) {
        if (!filePath.isEmpty() && !filePath.endsWith('/'))
            filePath += '/';
    } else {
        if (filePath.endsWith('/'))
            filePath.chop(1);
    }
}

bool QuaZipFileInfo::Private::equals(const Private &other) const
{
    return zipSystem == other.zipSystem && //0
        zipVersionMadeBy == other.zipVersionMadeBy && //1
        internalAttributes == other.internalAttributes && //2
        flags == other.flags && //3
        zipVersionNeeded == other.zipVersionNeeded && //4
        zipOptions == other.zipOptions && //5
        compressionMethod == other.compressionMethod && //6
        compressionStrategy == other.compressionStrategy && //7
        externalAttributes == other.externalAttributes && //8
        crc == other.crc && //9
        compressionLevel == other.compressionLevel && //22
        diskNumber == other.diskNumber && //10
        uncompressedSize == other.uncompressedSize && //11
        compressedSize == other.compressedSize && //12
        0 == memcmp(cryptKeys, other.cryptKeys, sizeof(cryptKeys)) && //13
        createTime == other.createTime && //14
        modifyTime == other.modifyTime && //15
        accessTime == other.accessTime && //16
        filePath == other.filePath && //17
        symLinkTarget == other.symLinkTarget && //21
        comment == other.comment && //18
        centralExtraFields == other.centralExtraFields && //19
        localExtraFields == other.localExtraFields; //20
}
