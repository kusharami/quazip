#ifndef QUA_ZIPFILEINFO_H
#define QUA_ZIPFILEINFO_H

/*
Copyright (C) 2005-2014 Sergey A. Tachenov

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

#include <QByteArray>
#include <QDateTime>
#include <QFile>

#include "quazipkeysgenerator.h"
#include "quazextrafield.h"

/// Information about a file inside archive (with zip64 support).
/** Call QuaZip::getCurrentFileInfo() or QuaZipFile::getFileInfo() to
 * fill this structure. */
class QUAZIP_EXPORT QuaZipFileInfo {
public:
    enum EntryType
    {
        File,
        Directory,
        SymLink
    };

    enum ZipOption : quint16
    {
        Encryption = 1,
        CompressionFlag1 = 1 << 1,
        CompressionFlag2 = 1 << 2,
        CompressionFlags = CompressionFlag1 | CompressionFlag2,
        NormalCompression = 0,
        MaximumCompression = CompressionFlag1,
        FastCompression = CompressionFlag2,
        SuperFastCompression = CompressionFlags,
        HasDataDescriptor = 1 << 3,
        Patch = 1 << 5,
        StrongEncryption = 1 << 6,
        Unicode = 1 << 11,
        LocalHeaderMasking = 1 << 13,
        CompatibleOptions =
            Encryption | CompressionFlags | HasDataDescriptor | Unicode
    };
    Q_DECLARE_FLAGS(ZipOptions, ZipOption)

    enum Attribute : quint8
    {
        ReadOnly = 0x01,
        Hidden = 0x02,
        System = 0x04,
        DirAttr = 0x10,
        Archived = 0x20,
        AllAttrs = ReadOnly | Hidden | System | DirAttr | Archived
    };
    Q_DECLARE_FLAGS(Attributes, Attribute)

    enum ContentAttribute
    {
        Text = 0x01
    };

    enum ZipSystem : quint8
    {
        OS_MSDOS = 0,
        OS_AMIGA = 1,
        OS_OPENVMS = 2,
        OS_UNIX = 3,
        OS_VMCMS = 4,
        OS_ATARI = 5,
        OS_OS2HPFS = 6,
        OS_MACINTOSH = 7,
        OS_ZSYSTEM = 8,
        OS_CPM = 9,
        OS_TOPS20 = 10,
        OS_WINDOWS_NTFS = 11,
        OS_QDOS = 12,
        OS_ACORN = 13,
        OS_WINDOWS_VFAT = 14,
        OS_MVS = 15,
        OS_BEOS = 16,
        OS_TANDEM = 17,
        OS_THEOS = 18,
        OS_MACOSX = 19
    };

    QuaZipFileInfo();
    QuaZipFileInfo(const QuaZipFileInfo &other);
    ~QuaZipFileInfo();

    const QString &filePath() const;
    void setFilePath(const QString &filePath);

    const QString &comment() const;
    void setComment(const QString &value);

    EntryType entryType() const;
    void setEntryType(EntryType value);

    inline bool isDir() const;
    inline bool isFile() const;
    inline bool isSymLink() const;

    QFile::Permissions permissions() const;
    void setPermissions(QFile::Permissions value);

    Attributes attributes() const;
    void setAttributes(Attributes value);

    qint64 uncompressedSize() const;
    void setUncompressedSize(qint64 size);

    qint64 compressedSize() const;
    void setCompressedSize(qint64 size);

    quint32 crc() const;
    void setCrc(quint32 value);

    quint16 compressionMethod() const;
    void setCompressionMethod(quint16 method);

    quint16 compressionStrategy() const;
    void setCompressionStrategy(quint16 value);

    int compressionLevel() const;
    void setCompressionLevel(int level);

    ZipOptions zipOptions() const;
    void setZipOptions(ZipOptions options);

    ZipSystem systemMadeBy() const;
    void setSystemMadeBy(ZipSystem value);

    quint8 zipVersionMadeBy() const;
    void setZipVersionMadeBy(quint8 spec);

    quint16 madeBy() const;
    void setMadeBy(quint16 value);

    quint16 zipVersionNeeded() const;
    void setZipVersionNeeded(quint16 value);

    quint16 internalAttributes();
    void setInternalAttributes(quint16 value);

    qint32 externalAttributes();
    void setExternalAttributes(qint32 value);

    int diskNumber() const;
    void setDiskNumber(int value);

    bool isRaw() const;
    void setIsRaw(bool value);

    bool isText() const;
    void setIsText(bool value);

    bool isEncrypted() const;
    void setIsEncrypted(bool value);

    const QDateTime &creationTime() const;
    void setCreationTime(const QDateTime &time);

    const QDateTime &modificationTime() const;
    void setModificationTime(const QDateTime &time);

    const QDateTime &lastAccessTime() const;
    void setLastAccessTime(const QDateTime &time);

    const QuaZipKeysGenerator::Keys &cryptKeys() const;
    void setCryptKeys(const QuaZipKeysGenerator::Keys &keys);

    /// The password will be erased from memory after
    /// cryptHeader and cryptKeys generation
    void setPassword(QByteArray *value);

    const QuaZExtraField::Map &centralExtraFields() const;
    void setCentralExtraFields(const QuaZExtraField::Map &map);

    const QuaZExtraField::Map &localExtraFields() const;
    void setLocalExtraFields(const QuaZExtraField::Map &map);

    QuaZipFileInfo &operator=(const QuaZipFileInfo &other);

    bool operator==(const QuaZipFileInfo &other) const;
    inline bool operator!=(const QuaZipFileInfo &other) const;

private:
    struct Private;
    QSharedDataPointer<Private> d;
};

bool QuaZipFileInfo::isDir() const
{
    return entryType() == Directory;
}

bool QuaZipFileInfo::isFile() const
{
    return entryType() == File;
}

bool QuaZipFileInfo::isSymLink() const
{
    return entryType() == SymLink;
}

bool QuaZipFileInfo::operator!=(const QuaZipFileInfo &other) const
{
    return !operator==(other);
}

#endif
