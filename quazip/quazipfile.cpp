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

#include "quazipfile.h"
#include "quazipfileinfo.h"
#include "quazutils.h"
#include "quaziptextcodec.h"

#include "zip.h"
#include "unzip.h"

#include <QDir>

using namespace std;

/// The implementation class for QuaZip.
/**
\internal

This class contains all the private stuff for the QuaZipFile class, thus
allowing to preserve binary compatibility between releases, the
technique known as the Pimpl (private implementation) idiom.
*/
class QuaZipFilePrivate {
    friend class QuaZipFile;

private:
    Q_DISABLE_COPY(QuaZipFilePrivate)
    /// The pointer to the associated QuaZipFile instance.
    QuaZipFile *q;
    /// The QuaZip object to work with.
    QuaZip *zip;
    /// Case sensitivity mode.
    QuaZip::CaseSensitivity caseSensitivity;
    QuaZipFileInfo fileInfo;
    QString useFilePath;

    /// Write position to keep track of.
    /**
      QIODevice::pos() is broken for non-seekable devices, so we need
      our own position.
      */
    qint64 writePos;
    /// Uncompressed size to write along with a raw file.
    qint64 uncompressedSize;
    /// Whether \ref zip points to an internal QuaZip instance.
    /**
      This is true if the archive was opened by name, rather than by
      supplying an existing QuaZip instance.
      */
    bool internal;
    /// The last error.
    int zipError;
    /// Resets \ref zipError.
    inline void clearZipError();
    /// Sets the zip error.
    /**
      This function is marked as const although it changes one field.
      This allows to call it from const functions that don't change
      anything by themselves.
      */
    void setZipError(int zipError);
    /// The constructor for the corresponding QuaZipFile constructor.
    inline QuaZipFilePrivate(QuaZipFile *q);
    /// The constructor for the corresponding QuaZipFile constructor.
    inline QuaZipFilePrivate(QuaZipFile *q, const QString &zipFilePath);
    /// The constructor for the corresponding QuaZipFile constructor.
    QuaZipFilePrivate(QuaZipFile *q, const QString &zipFilePath,
        const QString &filePath, QuaZip::CaseSensitivity cs);
    /// The constructor for the QuaZipFile constructor accepting a file name.
    QuaZipFilePrivate(QuaZipFile *q, QuaZip *zip);
    /// The destructor.
    ~QuaZipFilePrivate();

    bool initRead();
    bool initWrite();
};

QuaZipFile::QuaZipFile()
    : p(new QuaZipFilePrivate(this))
{
}

QuaZipFile::QuaZipFile(QObject *parent)
    : QIODevice(parent)
    , p(new QuaZipFilePrivate(this))
{
}

QuaZipFile::QuaZipFile(const QString &zipFilePath, QObject *parent)
    : QIODevice(parent)
    , p(new QuaZipFilePrivate(this, zipFilePath))
{
}

QuaZipFile::QuaZipFile(const QString &zipFilePath, const QString &filePath,
    QuaZip::CaseSensitivity cs, QObject *parent)
    : QIODevice(parent)
    , p(new QuaZipFilePrivate(this, zipFilePath, filePath, cs))
{
}

QuaZipFile::QuaZipFile(QuaZip *zip, QObject *parent)
    : QIODevice(parent)
    , p(new QuaZipFilePrivate(this, zip))
{
}

QuaZipFile::~QuaZipFile()
{
    if (isOpen())
        close();
    delete p;
}

QString QuaZipFile::zipFilePath() const
{
    return p->zip == nullptr ? QString() : p->zip->zipFilePath();
}

QuaZip *QuaZipFile::zip() const
{
    return p->internal ? nullptr : p->zip;
}

QString QuaZipFile::actualFilePath() const
{
    p->setZipError(UNZ_OK);
    if (p->zip == nullptr || isWritable())
        return p->fileInfo.filePath();

    QString name = p->zip->currentFilePath();
    if (name.isNull())
        p->setZipError(p->zip->getZipError());
    return name;
}

void QuaZipFile::setZipFilePath(const QString &zipFilePath)
{
    if (isOpen()) {
        qWarning(
            "QuaZipFile::setZipFilePath(): file is already open - can not set "
            "ZIP name");
        return;
    }
    if (p->zip != NULL && p->internal)
        delete p->zip;

    p->zip = new QuaZip(zipFilePath);
    p->internal = true;
}

void QuaZipFile::setZip(QuaZip *zip)
{
    if (isOpen()) {
        qWarning(
            "QuaZipFile::setZip(): file is already open - can not set ZIP");
        return;
    }
    if (p->zip != NULL && p->internal)
        delete p->zip;

    p->zip = zip;
    p->internal = false;
}

void QuaZipFile::setFilePath(const QString &filePath)
{
    if (p->zip == NULL) {
        qWarning("QuaZipFile::setFileName(): call setZipFilePath() first");
        return;
    }
    if (!p->internal) {
        qWarning("QuaZipFile::setFileName(): should not be used when not using "
                 "internal QuaZip");
        return;
    }
    if (isOpen()) {
        qWarning("QuaZipFile::setFileName(): can not set file name for already "
                 "opened file");
        return;
    }

    p->fileInfo.setFilePath(filePath);
    p->useFilePath = p->fileInfo.filePath();
}

void QuaZipFilePrivate::clearZipError()
{
    setZipError(UNZ_OK);
}

void QuaZipFilePrivate::setZipError(int zipError)
{
    this->zipError = zipError;
    if (zipError == UNZ_OK)
        q->setErrorString(QString());
    else
        q->setErrorString(QString("ZIP/UNZIP API error %1").arg(zipError));
}

QuaZipFilePrivate::QuaZipFilePrivate(QuaZipFile *q)
    : QuaZipFilePrivate(q, nullptr)
{
}

QuaZipFilePrivate::QuaZipFilePrivate(QuaZipFile *q, const QString &zipFilePath)
    : QuaZipFilePrivate(q, zipFilePath, QString(), QuaZip::csDefault)
{
}

QuaZipFilePrivate::QuaZipFilePrivate(QuaZipFile *q, const QString &zipFilePath,
    const QString &filePath, QuaZip::CaseSensitivity cs)
    : QuaZipFilePrivate(q, new QuaZip(zipFilePath))
{
    internal = true;
    fileInfo.setFilePath(filePath);
    useFilePath = fileInfo.filePath();
    caseSensitivity = cs;
}

QuaZipFilePrivate::QuaZipFilePrivate(QuaZipFile *q, QuaZip *zip)
    : q(q)
    , zip(zip)
    , caseSensitivity(QuaZip::csDefault)
    , writePos(0)
    , uncompressedSize(0)
    , internal(false)
    , zipError(UNZ_OK)
{
}

QuaZipFilePrivate::~QuaZipFilePrivate()
{
    if (internal)
        delete zip;
}

bool QuaZipFile::open(OpenMode mode)
{
    if (mode & (WriteOnly | Truncate)) {
        mode |= WriteOnly | Truncate;
    }
    mode |= Unbuffered;

    if (isOpen()) {
        qWarning("QuaZipFile is already open");
        Q_ASSERT(mode == openMode());
        return false;
    }

    if ((mode & ReadWrite) == ReadWrite) {
        p->zipError = UNZ_PARAMERROR;
        setErrorString(
            "Zip file should be opened in read-only or write-only mode");
        return false;
    }

    if (mode & Append) {
        p->zipError = UNZ_PARAMERROR;
        setErrorString("Append is not supported for a zip file.");
        return false;
    }

    setOpenMode(mode);

    if (mode & ReadOnly) {
        if (!p->initRead()) {
            setOpenMode(NotOpen);
            return false;
        }
    }

    if (mode & WriteOnly) {
        if (!p->initWrite()) {
            setOpenMode(NotOpen);
            return false;
        }
    }

    p->clearZipError();
    return QIODevice::open(mode);
}

void QuaZipFile::setPassword(QString *password)
{
    if (!password || password->isNull()) {
        p->fileInfo.setIsEncrypted(false);
        return;
    }

    QuaZipKeysGenerator keyGen(p->zip ? p->zip->passwordCodec() : nullptr);
    keyGen.addPassword(*password);
    p->fileInfo.setCryptKeys(keyGen.keys());
}

bool QuaZipFilePrivate::initRead()
{
    if (zip == nullptr) {
        zipError = UNZ_PARAMERROR;
        q->setErrorString("Zip archive is not set.");
        return false;
    }

    if (internal) {
        Q_ASSERT(!zip->isOpen());
        if (!zip->open(QuaZip::mdUnzip)) {
            setZipError(zip->getZipError());
            return false;
        }
    }

    if (zip->getMode() != QuaZip::mdUnzip) {
        zipError = UNZ_PARAMERROR;
        q->setErrorString("Zip archive is not opened for reading.");
        return false;
    }

    if (!useFilePath.isEmpty()) {
        if (!zip->setCurrentFile(useFilePath, caseSensitivity)) {
            setZipError(zip->getZipError());
            return false;
        }
    }

    if (!zip->hasCurrentFile()) {
        zipError = UNZ_PARAMERROR;
        q->setErrorString("File to read from Zip archive is not found.");
        return false;
    }

    setZipError(unzOpenCurrentFile4(zip->getUnzFile(), NULL, NULL,
        (int) fileInfo.isRaw(), fileInfo.cryptKeys()));
    return zipError == UNZ_OK;
}

bool QuaZipFilePrivate::initWrite()
{
    if (internal) {
        qWarning("QuaZipFile::open(): write mode is incompatible with "
                 "internal QuaZip approach");
        return false;
    }

    if (zip == NULL) {
        qWarning("QuaZipFile::open(): zip is NULL");
        return false;
    }

    switch (zip->getMode()) {
    case QuaZip::mdCreate:
    case QuaZip::mdAppend:
    case QuaZip::mdAdd: {
        auto zipOptions = fileInfo.zipOptions();
        if (!fileInfo.isRaw() &&
            (0 != (zipOptions & ~QuaZipFileInfo::CompatibleOptions))) {
            q->setErrorString("Some incompatible options used to compress ZIP");
            return false;
        }

        auto modTime = fileInfo.modificationTime();
        if (modTime.isNull()) {
            modTime = QDateTime::currentDateTimeUtc();
            fileInfo.setModificationTime(modTime);
        }

        auto accTime = fileInfo.lastAccessTime();
        if (accTime.isNull()) {
            accTime = QDateTime::currentDateTimeUtc();
            fileInfo.setLastAccessTime(accTime);
        }

        auto createTime = fileInfo.creationTime();
        if (createTime.isNull()) {
            createTime = QDateTime::currentDateTimeUtc();
            fileInfo.setCreationTime(createTime);
        }

        zipOptions.setFlag(QuaZipFileInfo::HasDataDescriptor,
            zip->isDataDescriptorWritingEnabled());

        if (zip->isDataDescriptorWritingEnabled()) {
            zipSetFlags(zip->getZipFile(), ZIP_WRITE_DATA_DESCRIPTOR);
        } else
            zipClearFlags(zip->getZipFile(), ZIP_WRITE_DATA_DESCRIPTOR);

        QByteArray extraLocal;
        QByteArray extraCentralDir;

        auto compatibility = zip->compatibilityFlags();
        fileInfo.setZipVersionMadeBy(45);
        auto attr = fileInfo.attributes();
        auto perm = fileInfo.permissions();

        bool isUnicodeFilePath = !QuaZUtils::isAscii(fileInfo.filePath());
        bool isUnicodeComment = !QuaZUtils::isAscii(fileInfo.comment());

        if (compatibility &
            (QuaZip::UnixCompatible | QuaZip::WindowsCompatible)) {
            if (compatibility & QuaZip::UnixCompatible) {
                fileInfo.setSystemMadeBy(QuaZipFileInfo::OS_UNIX);
            } else {
                fileInfo.setSystemMadeBy(QuaZipFileInfo::OS_WINDOWS_NTFS);
            }
            zipOptions.setFlag(QuaZipFileInfo::Unicode,
                !(compatibility & QuaZip::DOS_Compatible) &&
                    (isUnicodeFilePath || isUnicodeComment));
        } else {
            switch (int(compatibility)) {
            case QuaZip::DOS_Compatible:
                fileInfo.setSystemMadeBy(QuaZipFileInfo::OS_MSDOS);
                zipOptions &= ~QuaZipFileInfo::Unicode;
                break;

            case QuaZip::CustomCompatibility: {
                bool usedUnicodeFilePathCodec =
                    zip->fileNameCodec()->mibEnum() ==
                    QuaZipTextCodec::IANA_UTF8;
                bool usedUnicodeCommentCodec = zip->commentCodec()->mibEnum() ==
                    QuaZipTextCodec::IANA_UTF8;

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

        auto extraFields = fileInfo.centralExtraFields();
        zip->updateExtraFields(extraFields, fileInfo);
        fileInfo.setCentralExtraFields(extraFields);

        zip_fileinfo info_z;
        info_z.tmz_date.tm_year = modTime.date().year();
        info_z.tmz_date.tm_mon = modTime.date().month() - 1;
        info_z.tmz_date.tm_mday = modTime.date().day();
        info_z.tmz_date.tm_hour = modTime.time().hour();
        info_z.tmz_date.tm_min = modTime.time().minute();
        info_z.tmz_date.tm_sec = modTime.time().second();
        info_z.dosDate = 0;
        info_z.internal_fa = fileInfo.internalAttributes();
        info_z.external_fa = fileInfo.externalAttributes();
        auto compatibleFilePath = zip->compatibleFilePath(fileInfo.filePath());
        info_z.filename = compatibleFilePath.data();
        auto compatibleComment = zip->compatibleComment(fileInfo.comment());
        info_z.comment = compatibleComment.data();
        info_z.level = fileInfo.compressionLevel();
        info_z.raw = fileInfo.isRaw();
        info_z.crc = fileInfo.crc();
        info_z.flag = fileInfo.zipOptions();
        info_z.memLevel = MAX_MEM_LEVEL;
        info_z.windowBits = -MAX_WBITS;
        info_z.method = fileInfo.compressionMethod();
        info_z.uncompressed_size = fileInfo.uncompressedSize();
        info_z.versionMadeBy = fileInfo.madeBy();
        info_z.zip64 = zip->isZip64Enabled();
        info_z.strategy = fileInfo.compressionStrategy();

        setZipError(zipOpenNewFileInZipKeys(
            zip->getZipFile(), &info_z, fileInfo.cryptKeys()));

        if (zipError == UNZ_OK) {
            writePos = 0;
            return true;
        }
        break;
    }

    case QuaZip::mdUnzip:
    case QuaZip::mdNotOpen:
        zipError = UNZ_PARAMERROR;
        q->setErrorString("ZIP file is not writable");
        break;
    }

    return false;
}

bool QuaZipFile::isSequential() const
{
    if (isReadable())
        return p->zip->getIODevice()->isSequential();

    return true;
}

bool QuaZipFile::atEnd() const
{
    return bytesAvailable() == 0;
}

qint64 QuaZipFile::bytesAvailable() const
{
    if (!isOpen())
        return 0;

    if (p->zipError != UNZ_OK)
        return 0;

    if (isReadable()) {
        return size() - pos();
    }

    return 0;
}

qint64 QuaZipFile::size() const
{
    if (isReadable())
        return p->raw ? compressedSize() : uncompressedSize();

    if (isWritable()) {
        return p->writePos;
    }

    qWarning("QuaZipFile::size(): file is not open");
    return -1;
}

qint64 QuaZipFile::compressedSize() const
{
    unz_file_info64 info_z;
    p->setZipError(UNZ_OK);
    if (p->zip == NULL || p->zip->getMode() != QuaZip::mdUnzip)
        return -1;
    p->setZipError(unzGetCurrentFileInfo64(
        p->zip->getUnzFile(), &info_z, NULL, 0, NULL, 0, NULL, 0));
    if (p->zipError != UNZ_OK)
        return -1;
    return info_z.compressed_size;
}

qint64 QuaZipFile::uncompressedSize() const
{
    unz_file_info64 info_z;
    p->setZipError(UNZ_OK);
    if (p->zip == NULL || p->zip->getMode() != QuaZip::mdUnzip)
        return -1;
    p->setZipError(unzGetCurrentFileInfo64(
        p->zip->getUnzFile(), &info_z, NULL, 0, NULL, 0, NULL, 0));
    if (p->zipError != UNZ_OK)
        return -1;
    return info_z.uncompressed_size;
}

bool QuaZipFile::getFileInfo(QuaZipFileInfo &info)
{
    if (p->zip == NULL || p->zip->getMode() != QuaZip::mdUnzip)
        return false;
    p->zip->getCurrentFileInfo(info);
    p->setZipError(p->zip->getZipError());
    return p->zipError == UNZ_OK;
}

void QuaZipFile::close()
{
    p->clearZipError();
    if (p->zip == NULL || !p->zip->isOpen())
        return;
    if (!isOpen()) {
        qWarning("QuaZipFile::close(): file isn't open");
        return;
    }
    if (openMode() & ReadOnly)
        p->setZipError(unzCloseCurrentFile(p->zip->getUnzFile()));
    else if (openMode() & WriteOnly)
        if (isRaw())
            p->setZipError(zipCloseFileInZipRaw64(
                p->zip->getZipFile(), p->uncompressedSize, p->crc));
        else
            p->setZipError(zipCloseFileInZip(p->zip->getZipFile()));
    else {
        qWarning("Wrong open mode: %d", (int) openMode());
        return;
    }
    if (p->zipError == UNZ_OK)
        setOpenMode(QIODevice::NotOpen);
    else
        return;
    if (p->internal) {
        p->zip->close();
        p->setZipError(p->zip->getZipError());
    }
}

qint64 QuaZipFile::readData(char *data, qint64 maxSize)
{
    p->setZipError(UNZ_OK);
    qint64 bytesRead =
        unzReadCurrentFile(p->zip->getUnzFile(), data, (unsigned) maxSize);
    if (bytesRead < 0) {
        p->setZipError((int) bytesRead);
        return -1;
    }
    return bytesRead;
}

qint64 QuaZipFile::writeData(const char *data, qint64 maxSize)
{
    p->setZipError(ZIP_OK);
    p->setZipError(
        zipWriteInFileInZip(p->zip->getZipFile(), data, (uint) maxSize));
    if (p->zipError != ZIP_OK)
        return -1;
    else {
        p->writePos += maxSize;
        return maxSize;
    }
}

QString QuaZipFile::filePath() const
{
    return p->fileName;
}

QuaZip::CaseSensitivity QuaZipFile::caseSensitivity() const
{
    return p->caseSensitivity;
}

void QuaZipFile::setCaseSensitivity(QuaZip::CaseSensitivity cs)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change case sensitivity when open.");
        return;
    }

    p->caseSensitivity = cs;
}

bool QuaZipFile::isRaw() const
{
    return p->raw;
}

void QuaZipFile::setIsRaw(bool raw)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change mode when open.");
        return;
    }

    p->raw = raw;
}

int QuaZipFile::compressionLevel() const
{
    return p->level;
}

void QuaZipFile::setCompressionLevel(int value)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change mode when open.");
        return;
    }

    p->level = value;
}

int QuaZipFile::compressionMethod() const
{
    return p->method;
}

void QuaZipFile::setCompressionMethod(int value)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change mode when open.");
        return;
    }

    p->method = value;
}

int QuaZipFile::compressionStrategy() const
{
    return p->strategy;
}

void QuaZipFile::setCompressionStrategy(int value)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change mode when open.");
        return;
    }

    p->strategy = value;
}

int QuaZipFile::getZipError() const
{
    return p->zipError;
}

const QString &QuaZipFile::comment() const
{
    return p->fileInfo.comment();
}

void QuaZipFile::setComment(const QString &comment)
{
    p->fileInfo.setComment(comment);
}

const QuaZExtraField::Map &QuaZipFile::extraFields() const
{
    return p->fileInfo.centralExtraFields();
}

void QuaZipFile::setExtraFields(const QuaZExtraField::Map &map)
{
    p->fileInfo.setCentralExtraFields(map);
}
