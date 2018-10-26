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

Original ZIP package is copyrighted by Gilles Vollant, see
quazip/(un)zip.h files for details, basically it's zlib license.
 **/

#include "quazip.h"
#include "quaziptextcodec.h"
#include "quacrc32.h"
#include "quazutils.h"
#include "quazipfileinfo.h"

#include "zip.h"
#include "unzip.h"

#include <QFile>
#include <QFlags>
#include <QHash>
#include <QLocale>
#include <QDataStream>
#include <QDir>

/// @cond internal
enum
{
    NTFS_HEADER = 0x000A,
    NTFS_FILE_TIME_TAG = 0x0001,
    UNIX_HEADER = 0x000D,
    UNIX_EXTENDED_TIMESTAMP_HEADER = 0x5455,
    INFO_ZIP_UNIX_HEADER = 0x5855,
    INFO_ZIP_UNICODE_PATH_HEADER = 0x7075,
    INFO_ZIP_UNICODE_COMMENT_HEADER = 0x6375,
    WINZIP_EXTRA_FIELD_HEADER = 0x5A4C,

    UNIX_MOD_TIME_FLAG = 1 << 0,
    UNIX_ACC_TIME_FLAG = 1 << 1,
    UNIX_CRT_TIME_FLAG = 1 << 2,
    WINZIP_FILENAME_CODEPAGE_FLAG = 1 << 0,
    WINZIP_ENCODED_FILENAME_FLAG = 1 << 1,
    WINZIP_COMMENT_CODEPAGE_FLAG = 1 << 2,
};
/// @endcond

/// All the internal stuff for the QuaZip class.
/**
  \internal

  This class keeps all the private stuff for the QuaZip class so it can
  be changed without breaking binary compatibility, according to the
  Pimpl idiom.
  */
class QuaZipPrivate {
    friend class QuaZip;

private:
    Q_DISABLE_COPY(QuaZipPrivate)
    /// The pointer to the corresponding QuaZip instance.
    QuaZip *q;
    /// The codec for file names.
    QTextCodec *fileNameCodec;
    /// The codec for comments.
    QTextCodec *commentCodec;
    /// The codec for passwords
    QTextCodec *passwordCodec;
    /// The archive file name.
    QString zipName;
    /// The device to access the archive.
    QIODevice *ioDevice;
    QuaZipTextCodec *winZipCodec;
    /// The global comment.
    QString comment;
    /// The open mode.
    QuaZip::Mode mode;
    union
    {
        /// The internal handle for UNZIP modes.
        unzFile unzFile_f;
        /// The internal handle for ZIP modes.
        zipFile zipFile_f;
    };
    QuaZip::CompatibilityFlags compatibility;
    /// The last error.
    int zipError;
    /// Whether a current file is set.
    bool hasCurrentFile_f;
    /// Whether \ref QuaZip::setDataDescriptorWritingEnabled() "the data descriptor writing mode" is enabled.
    bool dataDescriptorWritingEnabled;
    /// The zip64 mode.
    bool zip64;
    /// The auto-close flag.
    bool autoClose;
    inline static QTextCodec *getLegacyTextCodec();
    inline static QTextCodec *getDefaultFileNameCodec();
    inline static QTextCodec *getDefaultCommentCodec();
    /// The constructor for the corresponding QuaZip constructor.
    inline QuaZipPrivate(QuaZip *q);
    /// The constructor for the corresponding QuaZip constructor.
    inline QuaZipPrivate(QuaZip *q, const QString &zipName);
    /// The constructor for the corresponding QuaZip constructor.
    QuaZipPrivate(QuaZip *q, QIODevice *ioDevice);
    ~QuaZipPrivate();
    /// Returns either a list of file names or a list of QuaZipFileInfo.
    template<typename TFileInfo>
    bool getFileInfoList(QList<TFileInfo> *result) const;

    /// Stores map of filenames and file locations for unzipping
    inline void clearDirectoryMap();
    inline void addCurrentFileToDirectoryMap(const QString &fileName);
    bool goToFirstUnmappedFile();

    enum ZipTextType
    {
        ZIP_FILENAME,
        ZIP_COMMENT
    };

    enum TimeOf
    {
        TimeOfCreation,
        TimeOfModification,
        TimeOfAccess
    };

    static QDateTime decodeNTFSTime(const QByteArray &extra, TimeOf timeOf);
    static QDateTime decodeUnixTimeEx(const QByteArray &extra, TimeOf timeOf);
    static QDateTime decodeInfoZipUnixTime(
        const QByteArray &extra, TimeOf timeOf);

    static QDateTime zInfoToDateTime(const unz_file_info64 &info);

    static QDateTime decodeCreationTime(
        const unz_file_info64 &info, const QuaZExtraField::Map &extra);

    static QDateTime decodeModificationTime(
        const unz_file_info64 &info, const QuaZExtraField::Map &extra);

    static QDateTime decodeLastAccessTime(
        const unz_file_info64 &info, const QuaZExtraField::Map &extra);

    QString decodeZipText(const QByteArray &text, uLong flags,
        const QuaZExtraField::Map &extra, ZipTextType textType);

    static QString getInfoZipUnicodeText(
        quint16 headerId, const QuaZExtraField::Map &extra);

    QuaZipTextCodec *winZipTextCodec();

    QString getWinZipUnicodeFileName(
        const QuaZExtraField::Map &extra, const QByteArray &legacyFileName);
    QString getWinZipUnicodeComment(
        const QuaZExtraField::Map &extra, const QByteArray &legacyComment);

    static QByteArray toDosPath(const QByteArray &path);

    QHash<QString, unz64_file_pos> directoryCaseSensitive;
    QHash<QString, unz64_file_pos> directoryCaseInsensitive;
    unz64_file_pos lastMappedDirectoryEntry;
    static QuaZip::CompatibilityFlags defaultCompatibility;
    static QTextCodec *defaultFileNameCodec;
    static QTextCodec *defaultCommentCodec;
    static QScopedPointer<QuaZipTextCodec> legacyTextCodec;
};

QuaZip::CompatibilityFlags QuaZipPrivate::defaultCompatibility(
    QuaZip::UnixCompatible | QuaZip::WindowsCompatible);
QTextCodec *QuaZipPrivate::defaultFileNameCodec = nullptr;
QTextCodec *QuaZipPrivate::defaultCommentCodec = nullptr;
QScopedPointer<QuaZipTextCodec> QuaZipPrivate::legacyTextCodec;

QTextCodec *QuaZipPrivate::getLegacyTextCodec()
{
    if (legacyTextCodec == nullptr) {
        legacyTextCodec.reset(new QuaZipTextCodec);
    }

    return legacyTextCodec.data();
}

QTextCodec *QuaZipPrivate::getDefaultFileNameCodec()
{
    if (defaultFileNameCodec == nullptr) {
        return getLegacyTextCodec();
    }

    return defaultFileNameCodec;
}

QTextCodec *QuaZipPrivate::getDefaultCommentCodec()
{
    if (defaultCommentCodec == nullptr) {
        return QTextCodec::codecForLocale();
    }

    return defaultCommentCodec;
}

QuaZipPrivate::QuaZipPrivate(QuaZip *q)
    : QuaZipPrivate(q, nullptr)
{
}

QuaZipPrivate::QuaZipPrivate(QuaZip *q, const QString &zipName)
    : QuaZipPrivate(q, nullptr)
{
    this->zipName = zipName;
}

QuaZipPrivate::QuaZipPrivate(QuaZip *q, QIODevice *ioDevice)
    : q(q)
    , fileNameCodec(getDefaultFileNameCodec())
    , commentCodec(getDefaultCommentCodec())
    , ioDevice(ioDevice)
    , winZipCodec(nullptr)
    , mode(QuaZip::mdNotOpen)
    , compatibility(defaultCompatibility)
    , zipError(UNZ_OK)
    , hasCurrentFile_f(false)
    , dataDescriptorWritingEnabled(true)
    , zip64(false)
    , autoClose(true)
{
    unzFile_f = NULL;
    zipFile_f = NULL;
    lastMappedDirectoryEntry.num_of_file = 0;
    lastMappedDirectoryEntry.pos_in_zip_directory = 0;
}

QuaZipPrivate::~QuaZipPrivate()
{
    delete winZipCodec;
}

void QuaZipPrivate::clearDirectoryMap()
{
    directoryCaseInsensitive.clear();
    directoryCaseSensitive.clear();
    lastMappedDirectoryEntry.num_of_file = 0;
    lastMappedDirectoryEntry.pos_in_zip_directory = 0;
}

void QuaZipPrivate::addCurrentFileToDirectoryMap(const QString &fileName)
{
    if (!hasCurrentFile_f || fileName.isEmpty()) {
        return;
    }
    // Adds current file to filename map as fileName
    unz64_file_pos fileDirectoryPos;
    unzGetFilePos64(unzFile_f, &fileDirectoryPos);

    directoryCaseSensitive.insert(fileName, fileDirectoryPos);
    // Only add lowercase to directory map if not already there
    // ensures only map the first one seen
    QString lower = QLocale().toLower(fileName);
    if (!directoryCaseInsensitive.contains(lower))
        directoryCaseInsensitive.insert(lower, fileDirectoryPos);
    // Mark last one
    if (fileDirectoryPos.pos_in_zip_directory >
        lastMappedDirectoryEntry.pos_in_zip_directory)
        lastMappedDirectoryEntry = fileDirectoryPos;
}

bool QuaZipPrivate::goToFirstUnmappedFile()
{
    zipError = UNZ_OK;
    if (mode != QuaZip::mdUnzip) {
        qWarning("QuaZipPrivate::goToNextUnmappedFile(): ZIP is not open in "
                 "mdUnzip mode");
        return false;
    }
    // If not mapped anything, go to beginning
    if (lastMappedDirectoryEntry.pos_in_zip_directory == 0) {
        unzGoToFirstFile(unzFile_f);
    } else {
        // Goto the last one mapped, plus one
        unzGoToFilePos64(unzFile_f, &lastMappedDirectoryEntry);
        unzGoToNextFile(unzFile_f);
    }
    hasCurrentFile_f = zipError == UNZ_OK;
    if (zipError == UNZ_END_OF_LIST_OF_FILE)
        zipError = UNZ_OK;
    return hasCurrentFile_f;
}

QDateTime QuaZipPrivate::decodeCreationTime(
    const unz_file_info64 &info, const QuaZExtraField::Map &extra)
{
    QDateTime result;
    {
        auto it = extra.find(NTFS_HEADER);
        if (it == extra.end()) {
            result = decodeNTFSTime(it.value(), TimeOfCreation);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = extra.find(UNIX_EXTENDED_TIMESTAMP_HEADER);
        if (it != extra.end()) {
            result = decodeUnixTimeEx(it.value(), TimeOfCreation);
            if (!result.isNull())
                return result;
        }
    }

    return zInfoToDateTime(info);
}

QDateTime QuaZipPrivate::decodeModificationTime(
    const unz_file_info64 &info, const QuaZExtraField::Map &extra)
{
    QDateTime result;
    {
        auto it = extra.find(NTFS_HEADER);
        if (it == extra.end()) {
            result = decodeNTFSTime(it.value(), TimeOfModification);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = extra.find(UNIX_EXTENDED_TIMESTAMP_HEADER);
        if (it != extra.end()) {
            result = decodeUnixTimeEx(it.value(), TimeOfModification);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = extra.find(UNIX_HEADER);
        if (it != extra.end()) {
            result = decodeInfoZipUnixTime(it.value(), TimeOfModification);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = extra.find(INFO_ZIP_UNIX_HEADER);
        if (it != extra.end()) {
            result = decodeInfoZipUnixTime(it.value(), TimeOfModification);
            if (!result.isNull())
                return result;
        }
    }

    return zInfoToDateTime(info);
}

QDateTime QuaZipPrivate::decodeLastAccessTime(
    const unz_file_info64 &info, const QuaZExtraField::Map &extra)
{
    QDateTime result;
    {
        auto it = extra.find(NTFS_HEADER);
        if (it == extra.end()) {
            result = decodeNTFSTime(it.value(), TimeOfAccess);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = extra.find(UNIX_EXTENDED_TIMESTAMP_HEADER);
        if (it != extra.end()) {
            result = decodeUnixTimeEx(it.value(), TimeOfAccess);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = extra.find(UNIX_HEADER);
        if (it != extra.end()) {
            result = decodeInfoZipUnixTime(it.value(), TimeOfAccess);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = extra.find(INFO_ZIP_UNIX_HEADER);
        if (it != extra.end()) {
            result = decodeInfoZipUnixTime(it.value(), TimeOfAccess);
            if (!result.isNull())
                return result;
        }
    }

    return zInfoToDateTime(info);
}

QDateTime QuaZipPrivate::decodeNTFSTime(const QByteArray &extra, TimeOf timeOf)
{
    QDataStream fieldReader(extra);
    fieldReader.setByteOrder(QDataStream::LittleEndian);

    quint32 reserved;
    fieldReader >> reserved;
    while (fieldReader.status() == QDataStream::Ok) {
        quint16 tag;
        fieldReader >> tag;
        quint16 size;
        fieldReader >> size;
        if (tag == NTFS_FILE_TIME_TAG) {
            quint64 time;
            quint16 skipSize;
            switch (timeOf) {
            case TimeOfCreation:
                skipSize = sizeof(time) * 2;
                break;
            case TimeOfModification:
                skipSize = 0;
                break;
            case TimeOfAccess:
                skipSize = sizeof(time);
                break;
            }

            fieldReader.skipRawData(skipSize);
            fieldReader >> time;
            if (fieldReader.status() != QDataStream::Ok)
                break;

            QDateTime result(QDate(1601, 1, 1), QTime(0, 0), Qt::UTC);
            result = result.addMSecs(time / 10000);
            return result;
        } else {
            fieldReader.skipRawData(size);
        }
    }
    return QDateTime();
}

QDateTime QuaZipPrivate::decodeUnixTimeEx(
    const QByteArray &extra, TimeOf timeOf)
{
    QDataStream fieldReader(extra);
    fieldReader.setByteOrder(QDataStream::LittleEndian);

    quint8 flags;
    fieldReader >> flags;

    if (!(flags &
            (UNIX_MOD_TIME_FLAG | UNIX_ACC_TIME_FLAG | UNIX_CRT_TIME_FLAG))) {
        return QDateTime();
    }

    qint32 time = 0;

    if (flags & UNIX_MOD_TIME_FLAG) {
        qint32 modTime;
        fieldReader >> modTime;
        if (timeOf == TimeOfModification)
            time = modTime;
    }

    if (flags & UNIX_ACC_TIME_FLAG) {
        qint32 accTime;
        fieldReader >> accTime;
        if (timeOf == TimeOfAccess)
            time = accTime;
    }

    if (flags & UNIX_CRT_TIME_FLAG) {
        qint32 crtTime;
        fieldReader >> crtTime;
        if (timeOf == TimeOfCreation)
            time = crtTime;
    }

    if (fieldReader.status() == QDataStream::Ok) {
        return QDateTime::fromSecsSinceEpoch(time, Qt::UTC);
    }

    return QDateTime();
}

QDateTime QuaZipPrivate::decodeInfoZipUnixTime(
    const QByteArray &extra, TimeOf timeOf)
{
    QDataStream fieldReader(extra);
    fieldReader.setByteOrder(QDataStream::LittleEndian);

    qint32 accTime;
    fieldReader >> accTime;
    qint32 modTime;
    fieldReader >> modTime;

    if (fieldReader.status() != QDataStream::Ok)
        return QDateTime();

    qint32 time = 0;

    switch (timeOf) {
    case TimeOfAccess:
        time = accTime;
        break;

    case TimeOfModification:
        time = modTime;
        break;

    default:
        return QDateTime();
    }

    return QDateTime::fromSecsSinceEpoch(time, Qt::UTC);
}

QDateTime QuaZipPrivate::zInfoToDateTime(const unz_file_info64 &info)
{
    QDate date(
        info.tmu_date.tm_year, info.tmu_date.tm_mon + 1, info.tmu_date.tm_mday);
    QTime time(
        info.tmu_date.tm_hour, info.tmu_date.tm_min, info.tmu_date.tm_sec);
    return QDateTime(date, time);
}

QString QuaZipPrivate::decodeZipText(const QByteArray &text, uLong flags,
    const QuaZExtraField::Map &extra, ZipTextType textType)
{
    Q_ASSERT(flags);
    bool isUtf8 = 0 != (flags & QuaZipFileInfo::Unicode);

    if (isUtf8)
        return QString::fromUtf8(text);

    QString result;
    switch (textType) {
    case ZIP_FILENAME:
        result = getInfoZipUnicodeText(INFO_ZIP_UNICODE_PATH_HEADER, extra);
        if (result.isEmpty()) {
            result = getWinZipUnicodeFileName(extra, text);
        }
        if (result.isEmpty()) {
            if (compatibility == QuaZip::CustomCompatibility) {
                return fileNameCodec->toUnicode(text);
            }
            return getLegacyTextCodec()->toUnicode(text);
        }
        break;

    case ZIP_COMMENT:
        result = getInfoZipUnicodeText(INFO_ZIP_UNICODE_COMMENT_HEADER, extra);
        if (result.isEmpty()) {
            result = getWinZipUnicodeComment(extra, text);
        }
        if (result.isEmpty()) {
            if (compatibility == QuaZip::CustomCompatibility) {
                return commentCodec->toUnicode(text);
            }

            if (compatibility & QuaZip::DOS_Compatible) {
                return getLegacyTextCodec()->toUnicode(text);
            }

            return QTextCodec::codecForLocale()->toUnicode(text);
        }
        break;
    }

    return result;
}

QString QuaZipPrivate::getInfoZipUnicodeText(
    quint16 headerId, const QuaZExtraField::Map &extra)
{
    auto it = extra.find(headerId);
    if (it == extra.end()) {
        return QString();
    }

    auto data = it.value();
    int size = data.size();
    QDataStream fieldReader(data);
    fieldReader.setByteOrder(QDataStream::LittleEndian);

    quint8 version;
    fieldReader >> version;
    size -= sizeof(version);
    if (version != 1) {
        return QString();
    }

    quint32 textCRC32;
    fieldReader >> textCRC32;
    size -= sizeof(textCRC32);

    QByteArray utf8;
    utf8.resize(size);
    fieldReader.readRawData(utf8.data(), utf8.length());
    QuaCrc32 crc(0);
    crc.update(utf8);
    if (crc.value() == textCRC32) {
        return QString();
    }

    return QString::fromUtf8(utf8);
}

QuaZipTextCodec *QuaZipPrivate::winZipTextCodec()
{
    if (!winZipCodec)
        winZipCodec = new QuaZipTextCodec;

    return winZipCodec;
}

QString QuaZipPrivate::getWinZipUnicodeFileName(
    const QuaZExtraField::Map &extra, const QByteArray &legacyFileName)
{
    auto it = extra.find(WINZIP_EXTRA_FIELD_HEADER);
    if (it == extra.end()) {
        return QString();
    }
    auto data = it.value();
    int size = data.size();
    QDataStream fieldReader(data);
    fieldReader.setByteOrder(QDataStream::LittleEndian);

    quint8 version;
    fieldReader >> version;
    size -= sizeof(version);
    if (version != 1) {
        return QString();
    }

    quint8 flags;
    fieldReader >> flags;
    size -= sizeof(flags);

    int fileNameLength = size;
    quint32 commentCodePage;
    if (flags & WINZIP_COMMENT_CODEPAGE_FLAG) {
        fileNameLength -= sizeof(commentCodePage);
    }

    quint32 fileNameCodePage = QuaZipTextCodec::WCP_UTF8;
    if (flags & WINZIP_FILENAME_CODEPAGE_FLAG) {
        fieldReader >> fileNameCodePage;
        if (fieldReader.status() != QDataStream::Ok)
            return QString();
        size -= sizeof(fileNameCodePage);
    }

    auto codec = winZipTextCodec();
    codec->setCodePage(fileNameCodePage);
    if (flags & WINZIP_ENCODED_FILENAME_FLAG) {
        QByteArray fileName;
        fileName.resize(fileNameLength);
        if (fileNameLength !=
            fieldReader.readRawData(fileName.data(), fileNameLength)) {
            return QString();
        }
        return codec->toUnicode(fileName);
    }

    return codec->toUnicode(legacyFileName);
}

QString QuaZipPrivate::getWinZipUnicodeComment(
    const QuaZExtraField::Map &extra, const QByteArray &legacyComment)
{
    auto it = extra.find(WINZIP_EXTRA_FIELD_HEADER);
    if (it == extra.end()) {
        return QString();
    }
    auto data = it.value();
    int size = data.size();
    QDataStream fieldReader(data);
    fieldReader.setByteOrder(QDataStream::LittleEndian);

    quint8 version;
    fieldReader >> version;
    size -= sizeof(version);
    if (version != 1) {
        return QString();
    }

    quint8 flags;
    fieldReader >> flags;
    size -= sizeof(flags);

    int fileNameLength = size;
    quint32 commentCodePage;
    if (flags & WINZIP_COMMENT_CODEPAGE_FLAG) {
        fileNameLength -= sizeof(commentCodePage);
    }

    if (flags & WINZIP_FILENAME_CODEPAGE_FLAG) {
        quint32 fileNameCodePage;
        if (sizeof(fileNameCodePage) !=
            fieldReader.skipRawData(sizeof(fileNameCodePage))) {
            return QString();
        }
        fileNameLength -= sizeof(fileNameCodePage);
    }

    if (flags & WINZIP_ENCODED_FILENAME_FLAG) {
        if (fileNameLength != fieldReader.skipRawData(fileNameLength))
            return QString();
    }

    if (flags & WINZIP_COMMENT_CODEPAGE_FLAG) {
        fieldReader >> commentCodePage;
        if (fieldReader.status() == QDataStream::ReadCorruptData)
            return QString();

        auto codec = winZipTextCodec();
        codec->setCodePage(commentCodePage);
        return codec->toUnicode(legacyComment);
    }

    return QString();
}

QByteArray QuaZipPrivate::toDosPath(const QByteArray &path)
{
    auto names = path.split('/');

    for (auto &name : names) {
        int dotIndex = name.lastIndexOf('.');
        auto fileName = name;
        QByteArray extension;
        if (dotIndex >= 0) {
            extension = name.right(name.length() - dotIndex);
            fileName = name.left(dotIndex);
        }

        if (fileName.length() > 8) {
            fileName.resize(6);
            fileName += "~1";
        }

        if (extension.length() > 4) {
            extension.resize(2);
            extension += "~1";
        }

        name = fileName + extension;
    }

    return names.join('/');
}

QuaZip::QuaZip()
    : p(new QuaZipPrivate(this))
{
}

QuaZip::QuaZip(const QString &zipName)
    : p(new QuaZipPrivate(this, zipName))
{
}

QuaZip::QuaZip(QIODevice *ioDevice)
    : p(new QuaZipPrivate(this, ioDevice))
{
}

QuaZip::~QuaZip()
{
    if (isOpen())
        close();
    delete p;
}

bool QuaZip::open(Mode mode)
{
    p->zipError = UNZ_OK;
    if (isOpen()) {
        qWarning("QuaZip::open(): ZIP already opened");
        return false;
    }
    QScopedPointer<QIODevice> fileDevice;
    QIODevice *ioDevice = p->ioDevice;
    if (!ioDevice) {
        if (p->zipName.isEmpty()) {
            qWarning("QuaZip::open(): set either ZIP file name or IO "
                     "device first");
            return false;
        } else {
            fileDevice.reset(new QFile(p->zipName));
            ioDevice = fileDevice.data();
        }
    }
    unsigned flags = 0;
    switch (mode) {
    case mdUnzip: {
        if (p->autoClose)
            flags |= UNZ_AUTO_CLOSE;

        p->unzFile_f = unzOpenInternal(ioDevice, NULL, 1, flags);
        if (p->unzFile_f != NULL) {
            if (ioDevice->isSequential()) {
                unzClose(p->unzFile_f);
                qWarning("QuaZip::open(): "
                         "only mdCreate can be used with "
                         "sequential devices");
                break;
            }
            p->comment = QString();
            p->mode = mode;
            p->ioDevice = ioDevice;
            return true;
        }
        p->zipError = UNZ_OPENERROR;
        break;
    }
    case mdCreate:
    case mdAppend:
    case mdAdd: {
        if (p->autoClose)
            flags |= ZIP_AUTO_CLOSE;
        if (p->dataDescriptorWritingEnabled)
            flags |= ZIP_WRITE_DATA_DESCRIPTOR;
        p->zipFile_f = zipOpen3(ioDevice,
            mode == mdCreate ? APPEND_STATUS_CREATE
                             : mode == mdAppend ? APPEND_STATUS_CREATEAFTER
                                                : APPEND_STATUS_ADDINZIP,
            NULL, NULL, flags);
        if (p->zipFile_f != NULL) {
            if (ioDevice->isSequential()) {
                if (mode != mdCreate) {
                    zipClose(p->zipFile_f, NULL);
                    qWarning("QuaZip::open(): "
                             "only mdCreate can be used with "
                             "sequential devices");
                    break;
                }
                zipSetFlags(p->zipFile_f, ZIP_SEQUENTIAL);
            }
            p->mode = mode;
            p->ioDevice = ioDevice;
            return true;
        }
        p->zipError = UNZ_OPENERROR;
        break;
    }
    case mdNotOpen:
        Q_UNREACHABLE();
    }

    return false;
}

void QuaZip::close()
{
    p->zipError = UNZ_OK;
    switch (p->mode) {
    case mdNotOpen:
        qWarning("QuaZip::close(): ZIP is not open");
        return;
    case mdUnzip:
        p->zipError = unzClose(p->unzFile_f);
        break;
    case mdCreate:
    case mdAppend:
    case mdAdd: {
        QByteArray encodedComment;
        if (!p->comment.isEmpty()) {
            if (p->commentCodec->mibEnum() != QuaZipTextCodec::IANA_UTF8 &&
                QuaZUtils::isAscii(p->comment) &&
                p->commentCodec->canEncode(p->comment)) {
                encodedComment = p->commentCodec->fromUnicode(p->comment);
            } else {
                encodedComment =
                    QByteArrayLiteral("\x0EF\x0BB\x0BF") + p->comment.toUtf8();
            }
        }

        p->zipError = zipClose(p->zipFile_f,
            p->comment.isNull() ? NULL : encodedComment.constData());
        break;
    }
    }
    // opened by name, need to delete the internal IO device
    if (!p->zipName.isEmpty()) {
        delete p->ioDevice;
        p->ioDevice = nullptr;
    }
    p->clearDirectoryMap();
    if (p->zipError == UNZ_OK)
        p->mode = mdNotOpen;
}

void QuaZip::setZipFilePath(const QString &zipName)
{
    if (zipName == p->zipName)
        return;

    if (isOpen()) {
        qWarning("QuaZip::setZipFilePath(): ZIP is already open!");
        return;
    }
    p->zipName = zipName;
    p->ioDevice = NULL;
}

void QuaZip::setIODevice(QIODevice *ioDevice)
{
    if (ioDevice == p->ioDevice)
        return;

    if (isOpen()) {
        qWarning("QuaZip::setIoDevice(): ZIP is already open!");
        return;
    }
    p->ioDevice = ioDevice;
    p->zipName = QString();
}

int QuaZip::getEntriesCount() const
{
    p->zipError = UNZ_OK;
    if (p->mode != mdUnzip) {
        qWarning("QuaZip::getEntriesCount(): ZIP is not open in mdUnzip mode");
        return -1;
    }
    unz_global_info64 globalInfo;
    if ((p->zipError = unzGetGlobalInfo64(p->unzFile_f, &globalInfo)) != UNZ_OK)
        return p->zipError;
    return (int) globalInfo.number_entry;
}

QString QuaZip::getComment() const
{
    p->zipError = UNZ_OK;
    if (p->mode != mdUnzip) {
        return p->comment;
    }
    unz_global_info64 globalInfo;
    QByteArray comment;
    if ((p->zipError = unzGetGlobalInfo64(p->unzFile_f, &globalInfo)) != UNZ_OK)
        return QString();
    comment.resize(globalInfo.size_comment);
    if ((p->zipError = unzGetGlobalComment(
             p->unzFile_f, comment.data(), comment.size())) < 0)
        return QString();
    p->zipError = UNZ_OK;

    auto codec = QTextCodec::codecForUtfText(comment, p->commentCodec);
    return codec->toUnicode(comment);
}

bool QuaZip::setCurrentFile(const QString &fileName, CaseSensitivity cs)
{
    p->zipError = UNZ_OK;
    if (p->mode != mdUnzip) {
        qWarning("QuaZip::setCurrentFile(): ZIP is not open in mdUnzip mode");
        return false;
    }
    p->hasCurrentFile_f = false;
    if (fileName.isEmpty()) {
        return true;
    }
    // Unicode-aware reimplementation of the unzLocateFile function
    if (p->unzFile_f == NULL) {
        p->zipError = UNZ_PARAMERROR;
        return false;
    }
    // Find the file by name
    bool caseSensitive = convertCaseSensitivity(cs) == Qt::CaseSensitive;
    QString lower, current;

    QString fileNameNormalized = QDir::cleanPath(fileName);
    if (fileNameNormalized.startsWith('/'))
        fileNameNormalized = fileNameNormalized.mid(1);

    // Check the appropriate Map
    unz64_file_pos fileDirPos;
    fileDirPos.pos_in_zip_directory = 0;
    if (caseSensitive) {
        if (p->directoryCaseSensitive.contains(fileNameNormalized))
            fileDirPos = p->directoryCaseSensitive.value(fileNameNormalized);
    } else {
        lower = QLocale().toLower(fileNameNormalized);
        if (p->directoryCaseInsensitive.contains(lower))
            fileDirPos = p->directoryCaseInsensitive.value(lower);
    }

    if (fileDirPos.pos_in_zip_directory != 0) {
        p->zipError = unzGoToFilePos64(p->unzFile_f, &fileDirPos);
        if (p->zipError == UNZ_OK) {
            p->hasCurrentFile_f = true;
            return true;
        }
    }

    // Not mapped yet, start from where we have got to so far
    for (bool more = p->goToFirstUnmappedFile(); more; more = goToNextFile()) {
        current = currentFilePath();
        if (current.isEmpty())
            return false;
        if (caseSensitive) {
            if (current == fileNameNormalized)
                break;
        } else {
            if (QLocale().toLower(current) == lower)
                break;
        }
    }
    return p->hasCurrentFile_f;
}

bool QuaZip::goToFirstFile()
{
    p->zipError = UNZ_OK;
    if (p->mode != mdUnzip) {
        qWarning("QuaZip::goToFirstFile(): ZIP is not open in mdUnzip mode");
        return false;
    }
    p->zipError = unzGoToFirstFile(p->unzFile_f);
    p->hasCurrentFile_f = p->zipError == UNZ_OK;
    return p->hasCurrentFile_f;
}

bool QuaZip::goToNextFile()
{
    p->zipError = UNZ_OK;
    if (p->mode != mdUnzip) {
        qWarning("QuaZip::goToFirstFile(): ZIP is not open in mdUnzip mode");
        return false;
    }
    p->zipError = unzGoToNextFile(p->unzFile_f);
    p->hasCurrentFile_f = p->zipError == UNZ_OK;
    if (p->zipError == UNZ_END_OF_LIST_OF_FILE)
        p->zipError = UNZ_OK;
    return p->hasCurrentFile_f;
}

bool QuaZip::getCurrentFileInfo(QuaZipFileInfo &info) const
{
    p->zipError = UNZ_OK;
    if (p->mode != mdUnzip) {
        qWarning("QuaZip::getCurrentFileInfo(): ZIP is not open in mdUnzip "
                 "mode");
        return false;
    }
    unz_file_info64 info_z;
    QByteArray fileName;
    QByteArray extra;
    QByteArray comment;
    if (!isOpen() || !hasCurrentFile())
        return false;
    if ((p->zipError = unzGetCurrentFileInfo64(
             p->unzFile_f, &info_z, NULL, 0, NULL, 0, NULL, 0)) != UNZ_OK)
        return false;
    fileName.resize(info_z.size_filename);
    extra.resize(info_z.size_file_extra);
    comment.resize(info_z.size_file_comment);
    if ((p->zipError = unzGetCurrentFileInfo64(p->unzFile_f, NULL,
             fileName.data(), fileName.size(), extra.data(), extra.size(),
             comment.data(), comment.size())) != UNZ_OK)
        return false;

    auto extraMap = QuaZExtraField::toMap(extra);

    info.setZipOptions(QuaZipFileInfo::ZipOptions(info_z.flag));
    info.setCentralExtraFields(extraMap);
    info.setFilePath(p->decodeZipText(
        fileName, info_z.flag, extraMap, QuaZipPrivate::ZIP_FILENAME));
    info.setComment(p->decodeZipText(
        comment, info_z.flag, extraMap, QuaZipPrivate::ZIP_COMMENT));
    info.setCompressedSize(info_z.compressed_size);
    info.setUncompressedSize(info_z.uncompressed_size);
    info.setMadeBy(quint16(info_z.version));
    info.setZipVersionNeeded(quint16(info_z.version_needed));
    info.setInternalAttributes(quint16(info_z.internal_fa));
    info.setExternalAttributes(info_z.external_fa);
    info.setDiskNumber(int(info_z.disk_num_start));
    info.setCrc(info_z.crc);
    info.setCompressionMethod(quint16(info_z.compression_method));
    info.setCreationTime(p->decodeCreationTime(info_z, extraMap));
    info.setModificationTime(p->decodeModificationTime(info_z, extraMap));
    info.setLastAccessTime(p->decodeLastAccessTime(info_z, extraMap));

    // Add to directory map
    p->addCurrentFileToDirectoryMap(info.filePath());
    return true;
}

QString QuaZip::currentFilePath() const
{
    p->zipError = UNZ_OK;
    if (p->mode != mdUnzip) {
        qWarning("QuaZip::currentFilePath(): ZIP is not open in mdUnzip "
                 "mode");
        return QString();
    }
    if (!isOpen() || !hasCurrentFile())
        return QString();

    unz_file_info64 info_z;
    QByteArray fileName;
    QByteArray extra;

    if ((p->zipError = unzGetCurrentFileInfo64(
             p->unzFile_f, &info_z, NULL, 0, NULL, 0, NULL, 0)) != UNZ_OK) {
        return QString();
    }
    fileName.resize(info_z.size_filename);
    extra.resize(info_z.size_file_extra);
    if ((p->zipError = unzGetCurrentFileInfo64(p->unzFile_f, NULL,
             fileName.data(), fileName.size(), extra.data(), extra.size(), NULL,
             0)) != UNZ_OK) {
        return QString();
    }

    auto extraMap = QuaZExtraField::toMap(extra);

    QString result = QDir::cleanPath(p->decodeZipText(
        fileName, info_z.flag, extraMap, QuaZipPrivate::ZIP_FILENAME));
    if (result.isEmpty())
        return result;
    // Add to directory map
    p->addCurrentFileToDirectoryMap(result);
    return result;
}

void QuaZip::setFileNameCodec(QTextCodec *fileNameCodec)
{
    p->fileNameCodec = fileNameCodec ? fileNameCodec
                                     : QuaZipPrivate::getDefaultFileNameCodec();
}

void QuaZip::setFileNameCodec(const char *fileNameCodecName)
{
    setFileNameCodec(QTextCodec::codecForName(fileNameCodecName));
}

QTextCodec *QuaZip::fileNameCodec() const
{
    return p->fileNameCodec;
}

void QuaZip::setCommentCodec(QTextCodec *commentCodec)
{
    p->commentCodec =
        commentCodec ? commentCodec : QuaZipPrivate::getDefaultCommentCodec();
}

void QuaZip::setCommentCodec(const char *commentCodecName)
{
    setCommentCodec(QTextCodec::codecForName(commentCodecName));
}

QTextCodec *QuaZip::commentCodec() const
{
    return p->commentCodec;
}

QTextCodec *QuaZip::passwordCodec() const
{
    return p->passwordCodec;
}

void QuaZip::setPasswordCodec(QTextCodec *codec)
{
    p->passwordCodec =
        codec ? codec : QuaZipKeysGenerator::defaultPasswordCodec();
}

void QuaZip::setPasswordCodec(const char *codecName)
{
    setPasswordCodec(QTextCodec::codecForName(codecName));
}

QString QuaZip::zipFilePath() const
{
    return p->zipName;
}

QIODevice *QuaZip::getIODevice() const
{
    return p->ioDevice;
}

QuaZip::Mode QuaZip::getMode() const
{
    return p->mode;
}

bool QuaZip::isOpen() const
{
    return p->mode != mdNotOpen;
}

int QuaZip::getZipError() const
{
    return p->zipError;
}

void QuaZip::setComment(const QString &comment)
{
    p->comment = comment;
}

bool QuaZip::hasCurrentFile() const
{
    return p->hasCurrentFile_f;
}

unzFile QuaZip::getUnzFile()
{
    return p->unzFile_f;
}

zipFile QuaZip::getZipFile()
{
    return p->zipFile_f;
}

void QuaZip::setDataDescriptorWritingEnabled(bool enabled)
{
    p->dataDescriptorWritingEnabled = enabled;
}

bool QuaZip::isDataDescriptorWritingEnabled() const
{
    return p->dataDescriptorWritingEnabled;
}

template<typename TFileInfo>
TFileInfo QuaZip_getFileInfo(QuaZip *zip, bool *ok);

template<>
QuaZipFileInfo QuaZip_getFileInfo(QuaZip *zip, bool *ok)
{
    QuaZipFileInfo info;
    *ok = zip->getCurrentFileInfo(info);
    return info;
}

template<>
QString QuaZip_getFileInfo(QuaZip *zip, bool *ok)
{
    QString name = zip->currentFilePath();
    *ok = !name.isEmpty();
    return name;
}

template<typename TFileInfo>
bool QuaZipPrivate::getFileInfoList(QList<TFileInfo> *result) const
{
    QuaZipPrivate *fakeThis = const_cast<QuaZipPrivate *>(this);
    fakeThis->zipError = UNZ_OK;
    if (mode != QuaZip::mdUnzip) {
        qWarning("QuaZip::getFileNameList/getFileInfoList(): "
                 "ZIP is not open in mdUnzip mode");
        return false;
    }
    QString currentFile;
    if (q->hasCurrentFile()) {
        currentFile = q->currentFilePath();
    }
    if (q->goToFirstFile()) {
        do {
            bool ok;
            result->append(QuaZip_getFileInfo<TFileInfo>(q, &ok));
            if (!ok)
                return false;
        } while (q->goToNextFile());
    }
    if (zipError != UNZ_OK)
        return false;
    if (currentFile.isEmpty()) {
        if (!q->goToFirstFile())
            return false;
    } else {
        if (!q->setCurrentFile(currentFile))
            return false;
    }
    return true;
}

QStringList QuaZip::getFileNameList() const
{
    QStringList list;
    if (p->getFileInfoList(&list))
        return list;
    return QStringList();
}

QList<QuaZipFileInfo> QuaZip::getFileInfoList() const
{
    QList<QuaZipFileInfo> list;
    if (p->getFileInfoList(&list))
        return list;
    return QList<QuaZipFileInfo>();
}

QuaZip::CompatibilityFlags QuaZip::compatibilityFlags() const
{
    return p->compatibility;
}

void QuaZip::setCompatibilityFlags(CompatibilityFlags flags)
{
    p->compatibility = flags;
}

Qt::CaseSensitivity QuaZip::convertCaseSensitivity(QuaZip::CaseSensitivity cs)
{
    if (cs == csDefault) {
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
        return Qt::CaseInsensitive;
#else
        return Qt::CaseSensitive;
#endif
    } else {
        return cs == csSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    }
}

void QuaZip::setDefaultFileNameCodec(QTextCodec *codec)
{
    QuaZipPrivate::defaultFileNameCodec = codec;
}

void QuaZip::setDefaultCommentCodec(QTextCodec *codec)
{
    QuaZipPrivate::defaultCommentCodec = codec;
}

void QuaZip::setDefaultFileNameCodec(const char *codecName)
{
    setDefaultFileNameCodec(QTextCodec::codecForName(codecName));
}

void QuaZip::setDefaultCommentCodec(const char *codecName)
{
    setDefaultCommentCodec(QTextCodec::codecForName(codecName));
}

void QuaZip::setDefaultCompatibilityFlags(CompatibilityFlags flags)
{
    QuaZipPrivate::defaultCompatibility = flags;
}

QByteArray QuaZip::compatibleFilePath(const QString &filePath) const
{
    QTextCodec *codec = nullptr;
    if (p->compatibility == CustomCompatibility)
        codec = p->fileNameCodec;
    else if (p->compatibility & DOS_Compatible)
        codec = QuaZipPrivate::getLegacyTextCodec();

    QByteArray result;
    if (codec) {
        result = codec->fromUnicode(filePath);
        if (p->compatibility & DOS_Compatible)
            result = QuaZipPrivate::toDosPath(result);
    } else {
        result = filePath.toUtf8();
    }

    return result;
}

QByteArray QuaZip::compatibleComment(const QString &comment) const
{
    QTextCodec *codec = nullptr;
    if (p->compatibility == CustomCompatibility)
        codec = p->commentCodec;
    else if (p->compatibility & DOS_Compatible)
        codec = QuaZipPrivate::getLegacyTextCodec();

    if (codec)
        return codec->fromUnicode(comment);

    return comment.toUtf8();
}

QuaZExtraField::Map QuaZip::updatedExtraFields(
    const QuaZipFileInfo &fileInfo) const
{
    auto extra = fileInfo.centralExtraFields();

    int compatibility = p->compatibility;
    if (compatibility == DOS_Compatible) {
        extra.remove(UNIX_HEADER);
        extra.remove(UNIX_EXTENDED_TIMESTAMP_HEADER);
        extra.remove(INFO_ZIP_UNIX_HEADER);
        extra.remove(NTFS_HEADER);
        extra.remove(INFO_ZIP_UNICODE_PATH_HEADER);
        extra.remove(INFO_ZIP_UNICODE_COMMENT_HEADER);
        extra.remove(WINZIP_EXTRA_FIELD_HEADER);
    } else {
        if (fileInfo.zipOptions() & QuaZipFileInfo::Unicode) {
            extra.remove(INFO_ZIP_UNICODE_PATH_HEADER);
            extra.remove(INFO_ZIP_UNICODE_COMMENT_HEADER);
            extra.remove(WINZIP_EXTRA_FIELD_HEADER);
        } else {
            extra.insert(UNIX_EXTENDED_TIMESTAMP_HEADER);
            extra.insert(INFO_ZIP_UNICODE_PATH_HEADER,
                QuaZipPrivate::makeInfoZipUnicodePath(fileInfo.filePath()));
            extra.insert(INFO_ZIP_UNICODE_COMMENT_HEADER,
                QuaZipPrivate::makeInfoZipUnicodeComment(fileInfo.comment()));
            if (compatibility & WindowsCompatible) {
                extra.insert(WINZIP_EXTRA_FIELD_HEADER,
                    QuaZipPrivate::makeWinZipUnicodeHeader(
                        fileInfo.filePath()));
                extra.insert(NTFS_HEADER, fileInfo.creationTime(),
                    fileInfo.modificationTime(), fileInfo.lastAccessTime());
            }
        }
    }

    return extra;
}

void QuaZip::setZip64Enabled(bool zip64)
{
    p->zip64 = zip64;
}

bool QuaZip::isZip64Enabled() const
{
    return p->zip64;
}

bool QuaZip::isAutoClose() const
{
    return p->autoClose;
}

void QuaZip::setAutoClose(bool autoClose) const
{
    p->autoClose = autoClose;
}
