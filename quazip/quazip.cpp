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
#include "private/quazipextrafields_p.h"

#include "zip.h"
#include "unzip.h"

#include <QFile>
#include <QFlags>
#include <QHash>
#include <QLocale>
#include <QDataStream>
#include <QDir>

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
    QTextCodec *filePathCodec;
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
    QuaZip::OpenMode mode;
    union
    {
        /// The internal handle for UNZIP modes.
        unzFile unzFile_f;
        /// The internal handle for ZIP modes.
        zipFile zipFile_f;
    };
    QuaZip::Compatibility compatibility;
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
    static QDateTime decodeUnixTime(
        const QByteArray &localExtra, TimeOf timeOf);
    static QDateTime decodeUnixTimeEx(const QByteArray &centralExtra,
        const QByteArray &localExtra, TimeOf timeOf);
    static QDateTime decodeInfoZipUnixTime(const QByteArray &centralExtra,
        const QByteArray &localExtra, TimeOf timeOf);

    static void storeUnixExtraFields(QuaZExtraField::Map &centralExtra,
        QuaZExtraField::Map &localExtra, const QDateTime &createTime,
        const QDateTime &modTime, const QDateTime &accTime,
        const QString &symLinkTarget);
    static void storeNTFSExtraFields(QuaZExtraField::Map &localExtra,
        const QDateTime &createTime, const QDateTime &modTime,
        const QDateTime &accTime);

    static quint64 dateTimeToNTFSTime(const QDateTime &from);
    static qint64 dateTimeToUnixTime(const QDateTime &from, qint32 &time32);
    static QDateTime zInfoToDateTime(const unz_file_info64 &info);
    static void fillTMZDate(tm_zip &out, const QDateTime &time);

    static QDateTime decodeCreationTime(const unz_file_info64 &info,
        const QuaZExtraField::Map &centralExtra,
        const QuaZExtraField::Map &localExtra);

    static QDateTime decodeModificationTime(const unz_file_info64 &info,
        const QuaZExtraField::Map &centralExtra,
        const QuaZExtraField::Map &localExtra);

    static QDateTime decodeLastAccessTime(const unz_file_info64 &info,
        const QuaZExtraField::Map &centralExtra,
        const QuaZExtraField::Map &localExtra);

    static QString decodeSymLinkTarget(const QuaZExtraField::Map &localExtra);

    QString decodeZipText(const QByteArray &text, uLong flags,
        const QuaZExtraField::Map &centralExtra, ZipTextType textType);

    QByteArray makeInfoZipText(const QString &text,
        const QByteArray &compatibleText, ZipTextType textType);
    void storeInfoZipFilePath(QuaZExtraField::Map &centralExtra,
        QuaZExtraField::Map &localExtra, const QString &filePath,
        const QByteArray &compatibleFilePath);
    void storeInfoZipComment(QuaZExtraField::Map &centralExtra,
        const QString &comment, const QByteArray &compatibleComment);
    void storeWinZipExtraFields(QuaZExtraField::Map &centralExtra,
        const QString &filePath, const QByteArray &compatibleFilePath);

    static QString getInfoZipUnicodeText(quint16 headerId,
        const QuaZExtraField::Map &extra, const QByteArray &legacyText);

    QuaZipTextCodec *winZipTextCodec();

    QString getWinZipUnicodeFileName(
        const QuaZExtraField::Map &extra, const QByteArray &legacyFileName);
    QString getWinZipUnicodeComment(
        const QuaZExtraField::Map &extra, const QByteArray &legacyComment);

    QByteArray compatibleFilePath(const QString &filePath) const;
    QTextCodec *compatibleFilePathCodec() const;
    QByteArray compatibleComment(const QString &comment) const;
    QTextCodec *compatibleCommentCodec() const;

    static QByteArray toDosPath(const QByteArray &path);
    static QString compatibleFilePath(const QString &path, QTextCodec *codec);

    QHash<QString, unz64_file_pos> directoryCaseSensitive;
    QHash<QString, unz64_file_pos> directoryCaseInsensitive;
    unz64_file_pos lastMappedDirectoryEntry;
    static QuaZip::Compatibility defaultCompatibility;
    static QTextCodec *defaultFileNameCodec;
    static QTextCodec *defaultCommentCodec;
    static QScopedPointer<QuaZipTextCodec> legacyTextCodec;
};

QuaZip::Compatibility QuaZipPrivate::defaultCompatibility(
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
    , filePathCodec(getDefaultFileNameCodec())
    , commentCodec(getDefaultCommentCodec())
    , passwordCodec(QuaZipKeysGenerator::defaultPasswordCodec())
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

QDateTime QuaZipPrivate::decodeCreationTime(const unz_file_info64 &info,
    const QuaZExtraField::Map &centralExtra,
    const QuaZExtraField::Map &localExtra)
{
    QDateTime result;
    {
        auto it = localExtra.find(NTFS_HEADER);
        if (it == localExtra.end()) {
            result = decodeNTFSTime(it.value(), TimeOfCreation);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = localExtra.find(UNIX_EXTENDED_TIMESTAMP_HEADER);
        auto cit = centralExtra.find(UNIX_EXTENDED_TIMESTAMP_HEADER);
        if (it != localExtra.end() && cit != centralExtra.end()) {
            result = decodeUnixTimeEx(cit.value(), it.value(), TimeOfCreation);
            if (!result.isNull())
                return result;
        }
    }

    return decodeModificationTime(info, centralExtra, localExtra);
}

QDateTime QuaZipPrivate::decodeModificationTime(const unz_file_info64 &info,
    const QuaZExtraField::Map &centralExtra,
    const QuaZExtraField::Map &localExtra)
{
    QDateTime result;
    {
        auto it = localExtra.find(NTFS_HEADER);
        if (it == localExtra.end()) {
            result = decodeNTFSTime(it.value(), TimeOfModification);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = localExtra.find(UNIX_EXTENDED_TIMESTAMP_HEADER);
        auto cit = centralExtra.find(UNIX_EXTENDED_TIMESTAMP_HEADER);
        if (it != localExtra.end() && cit != centralExtra.end()) {
            result =
                decodeUnixTimeEx(cit.value(), it.value(), TimeOfModification);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = localExtra.find(UNIX_HEADER);
        if (it != localExtra.end()) {
            result = decodeUnixTime(it.value(), TimeOfModification);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = localExtra.find(INFO_ZIP_UNIX_HEADER);
        auto cit = centralExtra.find(INFO_ZIP_UNIX_HEADER);
        if (it != localExtra.end() && cit != centralExtra.end()) {
            result = decodeInfoZipUnixTime(
                cit.value(), it.value(), TimeOfModification);
            if (!result.isNull())
                return result;
        }
    }

    return zInfoToDateTime(info);
}

QDateTime QuaZipPrivate::decodeLastAccessTime(const unz_file_info64 &info,
    const QuaZExtraField::Map &centralExtra,
    const QuaZExtraField::Map &localExtra)
{
    QDateTime result;
    {
        auto it = localExtra.find(NTFS_HEADER);
        if (it == localExtra.end()) {
            result = decodeNTFSTime(it.value(), TimeOfAccess);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = localExtra.find(UNIX_EXTENDED_TIMESTAMP_HEADER);
        auto cit = centralExtra.find(UNIX_EXTENDED_TIMESTAMP_HEADER);
        if (it != localExtra.end() && cit != centralExtra.end()) {
            result = decodeUnixTimeEx(cit.value(), it.value(), TimeOfAccess);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = localExtra.find(UNIX_HEADER);
        if (it != localExtra.end()) {
            result = decodeUnixTime(it.value(), TimeOfAccess);
            if (!result.isNull())
                return result;
        }
    }
    {
        auto it = localExtra.find(INFO_ZIP_UNIX_HEADER);
        auto cit = centralExtra.find(INFO_ZIP_UNIX_HEADER);
        if (it != localExtra.end() && cit != centralExtra.end()) {
            result =
                decodeInfoZipUnixTime(cit.value(), it.value(), TimeOfAccess);
            if (!result.isNull())
                return result;
        }
    }

    return zInfoToDateTime(info);
}

QString QuaZipPrivate::decodeSymLinkTarget(
    const QuaZExtraField::Map &localExtra)
{
    auto it = localExtra.find(UNIX_HEADER);
    if (it == localExtra.end())
        return QString();

    QByteArray data = it.value();
    int size = data.length();
    QDataStream localReader(data);
    localReader.setByteOrder(QDataStream::LittleEndian);

    qint32 accTime;
    Q_UNUSED(accTime);
    qint32 modTime;
    Q_UNUSED(modTime);
    quint16 uid;
    Q_UNUSED(uid);
    quint16 gid;
    Q_UNUSED(gid);
    int skipCount =
        sizeof(accTime) + sizeof(modTime) + sizeof(uid) + sizeof(gid);

    if (skipCount != localReader.skipRawData(skipCount))
        return QString();

    size -= skipCount;

    QByteArray symLinkTargetUtf8(size, Qt::Uninitialized);
    if (size != localReader.readRawData(symLinkTargetUtf8.data(), size))
        return QString();

    return QString::fromUtf8(symLinkTargetUtf8);
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

QDateTime QuaZipPrivate::decodeUnixTime(
    const QByteArray &localExtra, QuaZipPrivate::TimeOf timeOf)
{
    QDataStream localReader(localExtra);
    localReader.setByteOrder(QDataStream::LittleEndian);

    qint32 accTime;
    localReader >> accTime;
    qint32 modTime;
    localReader >> modTime;

    if (localReader.status() != QDataStream::Ok)
        return QDateTime();

    qint64 time = 0;

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

    return QDateTime::fromMSecsSinceEpoch(time * 1000, Qt::UTC);
}

QDateTime QuaZipPrivate::decodeUnixTimeEx(
    const QByteArray &centralExtra, const QByteArray &localExtra, TimeOf timeOf)
{
    QDataStream centralReader(centralExtra);
    centralReader.setByteOrder(QDataStream::LittleEndian);
    QDataStream localReader(localExtra);
    localReader.setByteOrder(QDataStream::LittleEndian);

    quint8 centralFlags;
    centralReader >> centralFlags;

    quint8 localFlags;
    localReader >> localFlags;

    do {
        if (centralReader.status() != QDataStream::Ok ||
            localReader.status() != QDataStream::Ok) {
            break;
        }

        if (!(localFlags &
                (UNIX_MOD_TIME_FLAG | UNIX_ACC_TIME_FLAG |
                    UNIX_CRT_TIME_FLAG))) {
            break;
        }

        qint64 time = 0;

        if (localFlags & UNIX_MOD_TIME_FLAG) {
            if (0 == (centralFlags & UNIX_MOD_TIME_FLAG))
                break;

            qint32 centralModTime;
            centralReader >> centralModTime;
            qint32 modTime;
            localReader >> modTime;
            if (centralModTime != modTime ||
                localReader.status() != QDataStream::Ok ||
                centralReader.status() != QDataStream::Ok) {
                break;
            }

            if (timeOf == TimeOfModification)
                time = modTime;
        }

        if (localFlags & UNIX_ACC_TIME_FLAG) {
            if (0 == (centralFlags & UNIX_ACC_TIME_FLAG))
                break;

            qint32 accTime;
            localReader >> accTime;
            if (localReader.status() != QDataStream::Ok)
                break;
            if (timeOf == TimeOfAccess)
                time = accTime;
        }

        if (localFlags & UNIX_CRT_TIME_FLAG) {
            if (0 == (centralFlags & UNIX_CRT_TIME_FLAG))
                break;

            qint32 crtTime;
            localReader >> crtTime;
            if (localReader.status() != QDataStream::Ok)
                break;
            if (timeOf == TimeOfCreation)
                time = crtTime;
        }

        if (localReader.status() == QDataStream::Ok) {
            return QDateTime::fromMSecsSinceEpoch(time * 1000, Qt::UTC);
        }
    } while (false);

    return QDateTime();
}

QDateTime QuaZipPrivate::decodeInfoZipUnixTime(
    const QByteArray &centralExtra, const QByteArray &localExtra, TimeOf timeOf)
{
    QDataStream centralReader(centralExtra);
    centralReader.setByteOrder(QDataStream::LittleEndian);
    QDataStream localReader(localExtra);
    localReader.setByteOrder(QDataStream::LittleEndian);

    qint32 locAccTime;
    localReader >> locAccTime;
    qint32 locModTime;
    localReader >> locModTime;

    qint32 accTime;
    centralReader >> accTime;
    qint32 modTime;
    centralReader >> modTime;

    if (accTime != locAccTime || modTime != locModTime ||
        centralReader.status() != QDataStream::Ok ||
        localReader.status() != QDataStream::Ok) {
        return QDateTime();
    }

    qint64 time = 0;

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

    return QDateTime::fromMSecsSinceEpoch(time * 1000, Qt::UTC);
}

void QuaZipPrivate::storeUnixExtraFields(QuaZExtraField::Map &centralExtra,
    QuaZExtraField::Map &localExtra, const QDateTime &createTime,
    const QDateTime &modTime, const QDateTime &accTime,
    const QString &symLinkTarget)
{
    if (modTime.isNull())
        return;

    qint32 modTime32;
    qint64 unixModTime = dateTimeToUnixTime(modTime, modTime32);
    qint32 accTime32 = modTime32;
    qint64 unixAccTime =
        accTime.isNull() ? unixModTime : dateTimeToUnixTime(accTime, accTime32);
    qint32 createTime32 = modTime32;
    qint64 unixCreateTime = createTime.isNull()
        ? unixModTime
        : dateTimeToUnixTime(createTime, createTime32);

    quint8 timeFlags = 0;
    if (accTime32 == unixAccTime)
        timeFlags |= UNIX_ACC_TIME_FLAG;
    if (modTime32 == unixModTime)
        timeFlags |= UNIX_MOD_TIME_FLAG;
    if (createTime32 == unixCreateTime)
        timeFlags |= UNIX_CRT_TIME_FLAG;

    if ((timeFlags & UNIX_ACC_TIME_FLAG) || (timeFlags & UNIX_MOD_TIME_FLAG) ||
        !symLinkTarget.isEmpty()) {
        QByteArray unixData;
        {
            QDataStream writer(&unixData, QIODevice::WriteOnly);
            writer.setByteOrder(QDataStream::LittleEndian);

            writer << accTime32;
            writer << modTime32;
            writer << quint16(0); //UID
            writer << quint16(0); //GID

            auto symLinkTargetUtf8 = symLinkTarget.toUtf8();
            writer.writeRawData(
                symLinkTargetUtf8.data(), symLinkTargetUtf8.length());
        }
        localExtra.insert(UNIX_HEADER, unixData);
    }

    if (timeFlags != 0) {
        QByteArray unixTimestamps;
        {
            QDataStream writer(&unixTimestamps, QIODevice::WriteOnly);
            writer.setByteOrder(QDataStream::LittleEndian);

            if (timeFlags & UNIX_MOD_TIME_FLAG) {
                if (modTime32 == accTime32)
                    timeFlags &= ~UNIX_ACC_TIME_FLAG;

                if (modTime32 == createTime32)
                    timeFlags &= ~UNIX_CRT_TIME_FLAG;
            }

            writer << timeFlags;
            if (timeFlags & UNIX_MOD_TIME_FLAG) {
                writer << modTime32;
            }
            centralExtra.insert(UNIX_EXTENDED_TIMESTAMP_HEADER, unixTimestamps);

            if (timeFlags & UNIX_ACC_TIME_FLAG) {
                writer << accTime32;
            }

            if (timeFlags & UNIX_CRT_TIME_FLAG) {
                writer << createTime32;
            }
        }
        localExtra.insert(UNIX_EXTENDED_TIMESTAMP_HEADER, unixTimestamps);
    }
}

void QuaZipPrivate::storeNTFSExtraFields(QuaZExtraField::Map &localExtra,
    const QDateTime &createTime, const QDateTime &modTime,
    const QDateTime &accTime)
{
    if (modTime.isNull())
        return;

    QByteArray ntfsData;
    {
        QDataStream writer(&ntfsData, QIODevice::WriteOnly);
        writer.setByteOrder(QDataStream::LittleEndian);

        writer << quint32(0); // reserved
        writer << quint16(NTFS_FILE_TIME_TAG);

        quint64 mNTFSTime = dateTimeToNTFSTime(modTime);
        quint64 cNTFSTime =
            createTime.isNull() ? mNTFSTime : dateTimeToNTFSTime(modTime);
        quint64 aNTFSTime =
            accTime.isNull() ? mNTFSTime : dateTimeToNTFSTime(accTime);

        writer << quint16(
            sizeof(mNTFSTime) + sizeof(aNTFSTime) + sizeof(cNTFSTime));
        writer << mNTFSTime;
        writer << aNTFSTime;
        writer << cNTFSTime;
    }

    localExtra.insert(NTFS_HEADER, ntfsData);
}

quint64 QuaZipPrivate::dateTimeToNTFSTime(const QDateTime &from)
{
    static const QDateTime ntfsTimeOrigin(
        QDate(1601, 1, 1), QTime(0, 0), Qt::UTC);

    return quint64(
        std::max<qint64>(0, ntfsTimeOrigin.msecsTo(from.toTimeSpec(Qt::UTC))) *
        10000);
}

qint64 QuaZipPrivate::dateTimeToUnixTime(const QDateTime &from, qint32 &time32)
{
    qint64 secs = from.toMSecsSinceEpoch() / 1000;
    if (secs < std::numeric_limits<qint32>::min())
        time32 = std::numeric_limits<qint32>::min();
    else if (secs > std::numeric_limits<qint32>::max())
        time32 = std::numeric_limits<qint32>::max();
    else
        time32 = qint32(secs);
    return secs;
}

QDateTime QuaZipPrivate::zInfoToDateTime(const unz_file_info64 &info)
{
    QDate date(int(info.tmu_date.tm_year), int(info.tmu_date.tm_mon + 1),
        int(info.tmu_date.tm_mday));
    QTime time(int(info.tmu_date.tm_hour), int(info.tmu_date.tm_min),
        int(info.tmu_date.tm_sec));
    return QDateTime(date, time);
}

void QuaZipPrivate::fillTMZDate(tm_zip &out, const QDateTime &dateTime)
{
    Q_ASSERT(dateTime.isValid());
    auto localTime = dateTime.toLocalTime();

    auto date = localTime.date();
    auto time = localTime.time();
    int year = date.year();

    if (year < 1980) {
        out.tm_year = 1980;
        out.tm_mon = 1;
        out.tm_mday = 1;
        out.tm_hour = 0;
        out.tm_min = 0;
        out.tm_sec = 0;
    } else if (year > 2107) {
        out.tm_year = 2107;
        out.tm_mon = 12;
        out.tm_mday = 31;
        out.tm_hour = 23;
        out.tm_min = 59;
        out.tm_sec = 59;
    } else {
        out.tm_year = uInt(year);
        out.tm_mon = uInt(date.month());
        out.tm_mday = uInt(date.day());
        out.tm_hour = uInt(time.hour());
        out.tm_min = uInt(time.minute());
        out.tm_sec = uInt(time.second());
    }
}

QString QuaZipPrivate::decodeZipText(const QByteArray &text, uLong flags,
    const QuaZExtraField::Map &centralExtra, ZipTextType textType)
{
    Q_ASSERT(flags);
    bool isUtf8 = 0 != (flags & QuaZipFileInfo::Unicode);

    if (isUtf8)
        return QString::fromUtf8(text);

    QString result;
    switch (textType) {
    case ZIP_FILENAME:
        result = getInfoZipUnicodeText(
            INFO_ZIP_UNICODE_PATH_HEADER, centralExtra, text);
        if (result.isEmpty()) {
            result = getWinZipUnicodeFileName(centralExtra, text);
        }
        if (result.isEmpty()) {
            if (compatibility == QuaZip::CustomCompatibility) {
                return filePathCodec->toUnicode(text);
            }
            return getLegacyTextCodec()->toUnicode(text);
        }
        break;

    case ZIP_COMMENT:
        result = getInfoZipUnicodeText(
            INFO_ZIP_UNICODE_COMMENT_HEADER, centralExtra, text);
        if (result.isEmpty()) {
            result = getWinZipUnicodeComment(centralExtra, text);
        }
        if (result.isEmpty()) {
            if (compatibility == QuaZip::CustomCompatibility) {
                return commentCodec->toUnicode(text);
            }

            if (compatibility & QuaZip::DosCompatible) {
                return getLegacyTextCodec()->toUnicode(text);
            }

            return QTextCodec::codecForLocale()->toUnicode(text);
        }
        break;
    }

    return result;
}

QByteArray QuaZipPrivate::makeInfoZipText(
    const QString &text, const QByteArray &compatibleText, ZipTextType textType)
{
    int headerId;
    switch (textType) {
    case ZIP_FILENAME:
        headerId = INFO_ZIP_UNICODE_PATH_HEADER;
        break;

    case ZIP_COMMENT:
        headerId = INFO_ZIP_UNICODE_COMMENT_HEADER;
        break;
    }

    auto utf8 = text.toUtf8();
    if (utf8 == compatibleText)
        return QByteArray();

    QByteArray result;
    {
        QDataStream writer(&result, QIODevice::WriteOnly);
        writer.setByteOrder(QDataStream::LittleEndian);

        writer << quint8(1);
        writer << zChecksum<QuaCrc32>(compatibleText);
        writer.writeRawData(utf8.data(), utf8.size());
    }
    return result;
}

void QuaZipPrivate::storeInfoZipFilePath(QuaZExtraField::Map &centralExtra,
    QuaZExtraField::Map &localExtra, const QString &filePath,
    const QByteArray &compatibleFilePath)
{
    auto infoZipFilePath = makeInfoZipText(
        filePath, compatibleFilePath, QuaZipPrivate::ZIP_FILENAME);
    if (!infoZipFilePath.isEmpty()) {
        centralExtra.insert(INFO_ZIP_UNICODE_PATH_HEADER, infoZipFilePath);
        localExtra.insert(INFO_ZIP_UNICODE_PATH_HEADER, infoZipFilePath);
    }
}

void QuaZipPrivate::storeInfoZipComment(QuaZExtraField::Map &centralExtra,
    const QString &comment, const QByteArray &compatibleComment)
{
    auto infoZipComment =
        makeInfoZipText(comment, compatibleComment, QuaZipPrivate::ZIP_COMMENT);
    if (!infoZipComment.isEmpty()) {
        centralExtra.insert(INFO_ZIP_UNICODE_COMMENT_HEADER, infoZipComment);
    }
}

void QuaZipPrivate::storeWinZipExtraFields(QuaZExtraField::Map &centralExtra,
    const QString &filePath, const QByteArray &compatibleFilePath)
{
    if (compatibility != QuaZip::CustomCompatibility &&
        0 == (compatibility & QuaZip::WindowsCompatible))
        return;

    quint8 flags = 0;
    quint32 filePathCodePage = 0;
    quint32 commentCodePage = 0;

    auto filePathCodec = compatibleFilePathCodec();
    int mib =
        filePathCodec ? filePathCodec->mibEnum() : QuaZipTextCodec::IANA_UTF8;
    filePathCodePage = QuaZipTextCodec::mibToCodePage(mib);
    auto commentCodec = compatibleCommentCodec();
    mib = commentCodec ? commentCodec->mibEnum() : QuaZipTextCodec::IANA_UTF8;
    commentCodePage = QuaZipTextCodec::mibToCodePage(mib);
    QByteArray filePathUtf8;
    if (compatibility == QuaZip::CustomCompatibility) {
        flags |= WINZIP_FILENAME_CODEPAGE_FLAG;
        flags |= WINZIP_COMMENT_CODEPAGE_FLAG;
    } else {
        filePathUtf8 = filePath.toUtf8();

        flags |= WINZIP_FILENAME_CODEPAGE_FLAG;
        if (filePathUtf8 != compatibleFilePath) {
            filePathCodePage = QuaZipTextCodec::WCP_UTF8;
            flags |= WINZIP_ENCODED_FILENAME_FLAG;
        }
    }

    if (flags == 0)
        return;

    QByteArray winZipExtra;
    {
        QDataStream writer(&winZipExtra, QIODevice::WriteOnly);
        writer.setByteOrder(QDataStream::LittleEndian);

        writer << quint8(1);
        writer << flags;

        if (flags & WINZIP_FILENAME_CODEPAGE_FLAG) {
            writer << filePathCodePage;
        }
        if (flags & WINZIP_ENCODED_FILENAME_FLAG) {
            writer.writeRawData(filePathUtf8.data(), filePathUtf8.length());
        }
        if (flags & WINZIP_COMMENT_CODEPAGE_FLAG) {
            writer << commentCodePage;
        }
    }
    centralExtra.insert(WINZIP_EXTRA_FIELD_HEADER, winZipExtra);
}

QString QuaZipPrivate::getInfoZipUnicodeText(quint16 headerId,
    const QuaZExtraField::Map &extra, const QByteArray &legacyText)
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

    QuaCrc32 crc(0);
    crc.update(legacyText);

    if (crc.value() != textCRC32)
        return QString();

    QByteArray utf8;
    if (size > 0) {
        utf8.resize(size);
        fieldReader.readRawData(utf8.data(), utf8.length());
    } else {
        utf8 = legacyText;
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

    if (fieldReader.status() != QDataStream::Ok)
        return QString();

    int fileNameLength = size;
    if (flags & WINZIP_COMMENT_CODEPAGE_FLAG) {
        quint32 commentCodePage;
        Q_UNUSED(commentCodePage);
        fileNameLength -= sizeof(commentCodePage);
    }

    quint32 fileNameCodePage = QuaZipTextCodec::WCP_UTF8;
    if (flags & WINZIP_FILENAME_CODEPAGE_FLAG) {
        fieldReader >> fileNameCodePage;
        if (fieldReader.status() != QDataStream::Ok)
            return QString();
        fileNameLength -= sizeof(fileNameCodePage);
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

    if (fieldReader.status() != QDataStream::Ok)
        return QString();

    int fileNameLength = size;
    quint32 commentCodePage;
    if (flags & WINZIP_COMMENT_CODEPAGE_FLAG) {
        fileNameLength -= sizeof(commentCodePage);
    }

    if (flags & WINZIP_FILENAME_CODEPAGE_FLAG) {
        quint32 fileNameCodePage;
        Q_UNUSED(fileNameCodePage);
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

QString QuaZipPrivate::compatibleFilePath(
    const QString &path, QTextCodec *codec)
{
    if (codec->canEncode(path))
        return path;

    auto split = path.split('/', QString::SkipEmptyParts);

    for (auto &sec : split) {
        if (codec->canEncode(sec))
            continue;

        int dotIndex = sec.lastIndexOf('.');
        QString name;
        QString extension;
        if (dotIndex >= 0) {
            name = sec.left(dotIndex);
            extension = sec.right(sec.length() - dotIndex);
        } else {
            name = sec;
        }

        if (!extension.isEmpty() && !codec->canEncode(extension)) {
            name = sec;
            extension.clear();
        }

        quint32 crc = zChecksum<QuaCrc32>(
            name.constData(), name.length() * sizeof(QChar));
        sec = QStringLiteral("%1%2").arg(crc, 8, 16, QChar('0')).arg(extension);
    }

    return split.join('/');
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

bool QuaZip::open(OpenMode mode)
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

        int openMode;
        switch (mode) {
        case mdCreate:
            openMode = APPEND_STATUS_CREATE;
            break;

        case mdAppend:
            openMode = APPEND_STATUS_CREATEAFTER;
            break;

        case mdAdd:
            openMode = APPEND_STATUS_ADDINZIP;
            break;

        default:
            Q_UNREACHABLE();
            return false;
        }

        p->zipFile_f = zipOpen3(ioDevice, openMode, NULL, NULL, flags);
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
        const auto &comment = p->comment;
        if (!comment.isEmpty()) {
            QTextCodec *commentCodec = nullptr;
            switch (int(p->compatibility)) {
            case DosCompatible:
            case CustomCompatibility:
                commentCodec = p->compatibleCommentCodec();
                break;

            default:
                if (QuaZUtils::isAscii(comment))
                    commentCodec = QuaZipPrivate::getLegacyTextCodec();
                break;
            }
            if (commentCodec && commentCodec->canEncode(comment)) {
                encodedComment = commentCodec->fromUnicode(comment);
            } else {
                encodedComment =
                    QByteArrayLiteral("\x0EF\x0BB\x0BF") + comment.toUtf8();
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

    if (ioDevice && ioDevice->isTextModeEnabled()) {
        p->zipError = ZIP_PARAMERROR;
        qWarning(
            "QuaZip::setIoDevice(): Zip should not be opened in text mode!");
    }
}

int QuaZip::entryCount() const
{
    p->zipError = UNZ_OK;
    if (p->mode != mdUnzip) {
        qWarning("QuaZip::getEntriesCount(): ZIP is not open in mdUnzip mode");
        return -1;
    }
    unz_global_info64 globalInfo;
    if ((p->zipError = unzGetGlobalInfo64(p->unzFile_f, &globalInfo)) != UNZ_OK)
        return p->zipError;

    if (globalInfo.number_entry > ZPOS64_T(std::numeric_limits<int>::max())) {
        p->zipError = ZIP_BADZIPFILE;
        return p->zipError;
    }

    return int(globalInfo.number_entry);
}

QString QuaZip::globalComment() const
{
    p->zipError = UNZ_OK;
    if (p->mode != mdUnzip) {
        return p->comment;
    }
    unz_global_info64 globalInfo;
    QByteArray comment;
    if ((p->zipError = unzGetGlobalInfo64(p->unzFile_f, &globalInfo)) != UNZ_OK)
        return QString();
    comment.resize(int(globalInfo.size_comment));
    if ((p->zipError = unzGetGlobalComment(
             p->unzFile_f, comment.data(), unsigned(comment.size()))) < 0)
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
    if (!isOpen() || !hasCurrentFile())
        return false;
    if ((p->zipError = unzGetCurrentFileInfo64(
             p->unzFile_f, &info_z, NULL, 0, NULL, 0, NULL, 0)) != UNZ_OK) {
        return false;
    }
    QByteArray fileName(int(info_z.size_filename), Qt::Uninitialized);
    QByteArray centralExtra(int(info_z.size_file_extra), Qt::Uninitialized);
    QByteArray comment(int(info_z.size_file_comment), Qt::Uninitialized);
    if ((p->zipError = unzGetCurrentFileInfo64(p->unzFile_f, NULL,
             fileName.data(), unsigned(fileName.size()), centralExtra.data(),
             unsigned(centralExtra.size()), comment.data(),
             unsigned(comment.size()))) != UNZ_OK) {
        return false;
    }

    if ((p->zipError = unzOpenCurrentFile(p->unzFile_f))) {
        return false;
    }

    int localExtraSize = unzGetLocalExtrafield(p->unzFile_f, NULL, 0);
    if (localExtraSize < 0) {
        p->zipError = localExtraSize;
        return false;
    }

    QByteArray localExtra(localExtraSize, Qt::Uninitialized);
    p->zipError = unzGetLocalExtrafield(
        p->unzFile_f, localExtra.data(), unsigned(localExtraSize));
    if (p->zipError != localExtraSize) {
        if (p->zipError >= 0)
            p->zipError = UNZ_ERRNO;
        return false;
    }

    if ((p->zipError = unzCloseCurrentFile(p->unzFile_f)))
        return false;

    auto centralExtraMap = QuaZExtraField::toMap(centralExtra);
    centralExtraMap.remove(ZIP64_HEADER);
    auto localExtraMap = QuaZExtraField::toMap(localExtra);
    localExtraMap.remove(ZIP64_HEADER);

    info.setMadeBy(quint16(info_z.version));
    info.setZipVersionNeeded(quint16(info_z.version_needed));
    info.setZipOptions(QuaZipFileInfo::ZipOptions(quint16(info_z.flag)));
    info.setCompressionMethod(quint16(info_z.compression_method));
    info.setCompressedSize(qint64(info_z.compressed_size));
    info.setUncompressedSize(qint64(info_z.uncompressed_size));
    info.setInternalAttributes(quint16(info_z.internal_fa));
    info.setExternalAttributes(qint32(info_z.external_fa));
    info.setDiskNumber(int(info_z.disk_num_start));
    info.setCrc(info_z.crc);
    info.setCompressionLevel(info.detectCompressionLevel());

    info.setCentralExtraFields(centralExtraMap);
    info.setLocalExtraFields(localExtraMap);
    info.setFilePath(p->decodeZipText(
        fileName, info_z.flag, centralExtraMap, QuaZipPrivate::ZIP_FILENAME));
    info.setComment(p->decodeZipText(
        comment, info_z.flag, centralExtraMap, QuaZipPrivate::ZIP_COMMENT));

    info.setCreationTime(QuaZipPrivate::decodeCreationTime(
        info_z, centralExtraMap, localExtraMap));
    info.setModificationTime(QuaZipPrivate::decodeModificationTime(
        info_z, centralExtraMap, localExtraMap));
    info.setLastAccessTime(QuaZipPrivate::decodeLastAccessTime(
        info_z, centralExtraMap, localExtraMap));
    info.setSymLinkTarget(QuaZipPrivate::decodeSymLinkTarget(localExtraMap));

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

    if ((p->zipError = unzGetCurrentFileInfo64(
             p->unzFile_f, &info_z, NULL, 0, NULL, 0, NULL, 0)) != UNZ_OK) {
        return QString();
    }
    QByteArray fileName(int(info_z.size_filename), Qt::Uninitialized);
    QByteArray centralExtra(int(info_z.size_file_extra), Qt::Uninitialized);

    if ((p->zipError = unzGetCurrentFileInfo64(p->unzFile_f, NULL,
             fileName.data(), unsigned(fileName.size()), centralExtra.data(),
             unsigned(centralExtra.size()), NULL, 0)) != UNZ_OK) {
        return QString();
    }

    auto extraMap = QuaZExtraField::toMap(centralExtra);

    QString result = QDir::cleanPath(p->decodeZipText(
        fileName, info_z.flag, extraMap, QuaZipPrivate::ZIP_FILENAME));

    // Add to directory map
    p->addCurrentFileToDirectoryMap(result);
    return result;
}

void QuaZip::setFilePathCodec(QTextCodec *filePathCodec)
{
    p->filePathCodec = filePathCodec ? filePathCodec
                                     : QuaZipPrivate::getDefaultFileNameCodec();
}

void QuaZip::setFilePathCodec(const char *filePathCodecName)
{
    setFilePathCodec(QTextCodec::codecForName(filePathCodecName));
}

QTextCodec *QuaZip::filePathCodec() const
{
    return p->filePathCodec;
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

QIODevice *QuaZip::ioDevice() const
{
    return p->ioDevice;
}

QuaZip::OpenMode QuaZip::openMode() const
{
    return p->mode;
}

bool QuaZip::isOpen() const
{
    return p->mode != mdNotOpen;
}

int QuaZip::zipError() const
{
    return p->zipError;
}

void QuaZip::setGlobalComment(const QString &comment)
{
    p->comment = comment;
}

bool QuaZip::hasCurrentFile() const
{
    return p->hasCurrentFile_f;
}

void *QuaZip::getUnzFile() const
{
    return p->unzFile_f;
}

void *QuaZip::getZipFile() const
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
            result->push_back(QuaZip_getFileInfo<TFileInfo>(q, &ok));
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

QStringList QuaZip::filePathList() const
{
    QStringList list;
    if (p->getFileInfoList(&list))
        return list;
    return QStringList();
}

QList<QuaZipFileInfo> QuaZip::fileInfoList() const
{
    QList<QuaZipFileInfo> list;
    if (p->getFileInfoList(&list))
        return list;
    return QList<QuaZipFileInfo>();
}

QuaZip::Compatibility QuaZip::compatibility() const
{
    return p->compatibility;
}

void QuaZip::setCompatibility(Compatibility value)
{
    p->compatibility = value;
}

Qt::CaseSensitivity QuaZip::convertCaseSensitivity(QuaZip::CaseSensitivity cs)
{
    if (cs == csDefault) {
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
        return Qt::CaseInsensitive;
#else
        return Qt::CaseSensitive;
#endif
    }
    return cs == csSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
}

void QuaZip::setDefaultFilePathCodec(QTextCodec *codec)
{
    QuaZipPrivate::defaultFileNameCodec = codec;
}

QTextCodec *QuaZip::defaultCommentCodec()
{
    return QuaZipPrivate::getDefaultCommentCodec();
}

void QuaZip::setDefaultCommentCodec(QTextCodec *codec)
{
    QuaZipPrivate::defaultCommentCodec = codec;
}

void QuaZip::setDefaultFilePathCodec(const char *codecName)
{
    setDefaultFilePathCodec(QTextCodec::codecForName(codecName));
}

void QuaZip::setDefaultCommentCodec(const char *codecName)
{
    setDefaultCommentCodec(QTextCodec::codecForName(codecName));
}

QTextCodec *QuaZip::defaultPasswordCodec()
{
    return QuaZipKeysGenerator::defaultPasswordCodec();
}

void QuaZip::setDefaultPasswordCodec(QTextCodec *codec)
{
    QuaZipKeysGenerator::setDefaultPasswordCodec(codec);
}

void QuaZip::setDefaultCompatibility(Compatibility flags)
{
    QuaZipPrivate::defaultCompatibility = flags;
}

QuaZip::Compatibility QuaZip::defaultCompatibility()
{
    return QuaZipPrivate::defaultCompatibility;
}

QByteArray QuaZipPrivate::compatibleFilePath(const QString &filePath) const
{
    QTextCodec *codec = compatibleFilePathCodec();

    QByteArray result;
    if (codec) {
        result = codec->fromUnicode(compatibleFilePath(filePath, codec));
        if (compatibility & QuaZip::DosCompatible)
            result = toDosPath(result);
    } else {
        result = filePath.toUtf8();
    }

    return result;
}

QTextCodec *QuaZipPrivate::compatibleFilePathCodec() const
{
    if (compatibility == QuaZip::CustomCompatibility)
        return filePathCodec;

    if (compatibility & QuaZip::DosCompatible)
        return getLegacyTextCodec();

    return nullptr;
}

QByteArray QuaZipPrivate::compatibleComment(const QString &comment) const
{
    QTextCodec *codec = compatibleCommentCodec();

    if (codec) {
        if (compatibility != QuaZip::DosCompatible &&
            !codec->canEncode(comment)) {
            return QByteArray();
        }
        return codec->fromUnicode(comment);
    }

    return comment.toUtf8();
}

QTextCodec *QuaZipPrivate::compatibleCommentCodec() const
{
    switch (int(compatibility)) {
    case QuaZip::CustomCompatibility:
        return commentCodec;

    case QuaZip::DosCompatible:
        return getLegacyTextCodec();
    }

    return nullptr;
}

void QuaZip::fillZipInfo(zip_fileinfo_s &zipInfo, QuaZipFileInfo &fileInfo,
    QByteArray &compatibleFilePath, QByteArray &compatibleComment) const
{
    auto zipOptions = fileInfo.zipOptions();

    auto modTime = fileInfo.modificationTime();
    if (modTime.isNull()) {
        modTime = QDateTime::currentDateTimeUtc();
        fileInfo.setModificationTime(modTime);
    }

    zipOptions.setFlag(
        QuaZipFileInfo::HasDataDescriptor, isDataDescriptorWritingEnabled());

    if (isDataDescriptorWritingEnabled()) {
        zipSetFlags(getZipFile(), ZIP_WRITE_DATA_DESCRIPTOR);
    } else
        zipClearFlags(getZipFile(), ZIP_WRITE_DATA_DESCRIPTOR);

    int compatibility = p->compatibility;
    fileInfo.setZipVersionMadeBy(45);
    auto attr = fileInfo.attributes();
    auto perm = fileInfo.permissions();

    bool isUnicodeFilePath = !QuaZUtils::isAscii(fileInfo.filePath());
    bool isUnicodeComment = !QuaZUtils::isAscii(fileInfo.comment());

    if (compatibility & (QuaZip::UnixCompatible | QuaZip::WindowsCompatible)) {
        if (compatibility & QuaZip::UnixCompatible) {
            fileInfo.setSystemMadeBy(QuaZipFileInfo::OS_UNIX);
        } else if (compatibility & QuaZip::DosCompatible) {
            fileInfo.setSystemMadeBy(QuaZipFileInfo::OS_MSDOS);
        } else {
            fileInfo.setSystemMadeBy(QuaZipFileInfo::OS_WINDOWS_NTFS);
        }
        zipOptions.setFlag(QuaZipFileInfo::Unicode,
            !(compatibility & QuaZip::DosCompatible) &&
                (isUnicodeFilePath || isUnicodeComment));
    } else {
        switch (compatibility) {
        case QuaZip::DosCompatible:
            fileInfo.setSystemMadeBy(QuaZipFileInfo::OS_MSDOS);
            zipOptions &= ~QuaZipFileInfo::Unicode;
            break;

        case QuaZip::CustomCompatibility: {
            bool usedUnicodeFilePathCodec =
                filePathCodec()->mibEnum() == QuaZipTextCodec::IANA_UTF8;
            bool usedUnicodeCommentCodec =
                commentCodec()->mibEnum() == QuaZipTextCodec::IANA_UTF8;

            zipOptions.setFlag(QuaZipFileInfo::Unicode,
                usedUnicodeCommentCodec && usedUnicodeFilePathCodec &&
                    (isUnicodeComment || isUnicodeFilePath));
            break;
        }
        }
    }

    fileInfo.setZipOptions(zipOptions);
    fileInfo.setAttributes(attr);
    fileInfo.setPermissions(perm);

    if (zipOptions & QuaZipFileInfo::Unicode) {
        compatibleFilePath = fileInfo.filePath().toUtf8();
        compatibleComment = fileInfo.comment().toUtf8();
    } else {
        compatibleFilePath = p->compatibleFilePath(fileInfo.filePath());

        if (isUnicodeComment && compatibility != DosCompatible) {
            //comment will be stored in extra fields only
            compatibleComment.clear();
        } else {
            compatibleComment = p->compatibleComment(fileInfo.comment());
        }
    }

    QuaZipPrivate::fillTMZDate(zipInfo.tmz_date, modTime);
    zipInfo.dosDate = 0;
    zipInfo.internal_fa = fileInfo.internalAttributes();
    zipInfo.external_fa = uLong(fileInfo.externalAttributes());
    zipInfo.filename = compatibleFilePath.data();
    zipInfo.comment = compatibleComment.data();
    zipInfo.level = fileInfo.compressionLevel();
    zipInfo.raw = fileInfo.isRaw();
    zipInfo.crc = fileInfo.crc();
    zipInfo.flag = fileInfo.zipOptions();
    zipInfo.memLevel = MAX_MEM_LEVEL;
    zipInfo.windowBits = -MAX_WBITS;
    zipInfo.method = fileInfo.compressionMethod();
    zipInfo.uncompressed_size = ZPOS64_T(fileInfo.uncompressedSize());
    zipInfo.versionMadeBy = fileInfo.madeBy();
    zipInfo.versionNeeded = fileInfo.zipVersionNeeded();
    zipInfo.zip64 = isZip64Enabled();
    zipInfo.strategy = fileInfo.compressionStrategy();

    auto localExtra = fileInfo.localExtraFields();
    auto centralExtra = fileInfo.centralExtraFields();

    QuaZExtraField::Map *EXTRA[] = {&localExtra, &centralExtra};
    for (auto map : EXTRA) {
        map->remove(UNIX_HEADER);
        map->remove(UNIX_EXTENDED_TIMESTAMP_HEADER);
        map->remove(INFO_ZIP_UNIX_HEADER);
        map->remove(NTFS_HEADER);
        map->remove(INFO_ZIP_UNICODE_PATH_HEADER);
        map->remove(INFO_ZIP_UNICODE_COMMENT_HEADER);
        map->remove(WINZIP_EXTRA_FIELD_HEADER);
    }
    if (compatibility != DosCompatible) {
        if (!(fileInfo.zipOptions() & QuaZipFileInfo::Unicode)) {
            if (compatibility != CustomCompatibility ||
                !filePathCodec()->canEncode(fileInfo.filePath())) {
                p->storeInfoZipFilePath(centralExtra, localExtra,
                    fileInfo.filePath(), compatibleFilePath);
            }

            if (compatibility != CustomCompatibility ||
                (!fileInfo.comment().isEmpty() &&
                    compatibleComment.isEmpty())) {
                p->storeInfoZipComment(
                    centralExtra, fileInfo.comment(), compatibleComment);
            }
            p->storeWinZipExtraFields(
                centralExtra, fileInfo.filePath(), compatibleFilePath);
        }

        auto &createTime = fileInfo.creationTime();
        auto &modTime = fileInfo.modificationTime();
        auto &accTime = fileInfo.lastAccessTime();

        if (0 != (compatibility & UnixCompatible) ||
            (compatibility == CustomCompatibility &&
                QuaZipFileInfo::isUnixHost(fileInfo.systemMadeBy()))) {
            QuaZipPrivate::storeUnixExtraFields(centralExtra, localExtra,
                createTime, modTime, accTime, fileInfo.symLinkTarget());
        }

        QuaZipPrivate::storeNTFSExtraFields(
            localExtra, createTime, modTime, accTime);
    }

    fileInfo.setCentralExtraFields(centralExtra);
    fileInfo.setLocalExtraFields(localExtra);
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

QTextCodec *QuaZip::defaultFilePathCodec()
{
    return QuaZipPrivate::getDefaultFileNameCodec();
}
