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

enum
{
    SEEK_BUFFER_SIZE = 32768
};

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
    QByteArray seekBuffer;

    /// Write position to keep track of.
    /**
      QIODevice::pos() is broken for non-seekable devices, so we need
      our own position.
      */
    ZPOS64_T writePos;
    /// Whether \ref zip points to an internal QuaZip instance.
    /**
      This is true if the archive was opened by name, rather than by
      supplying an existing QuaZip instance.
      */
    bool internal;
    bool fetchFileInfo;
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

    bool initFileInfo();
    QIODevice::OpenMode initRead(QIODevice::OpenMode mode);
    QIODevice::OpenMode initWrite(QIODevice::OpenMode mode);

    bool seekInternal(qint64 newPos);
    qint64 readInternal(char *data, qint64 maxlen);
    qint64 writeInternal(const char *data, qint64 maxlen);
};

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
    return p->zip;
}

QString QuaZipFile::actualFilePath() const
{
    return fileInfo().filePath();
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
    if (isOpen()) {
        qWarning("QuaZipFile::setFilePath(): can not set file name for already "
                 "opened file");
        return;
    }

    if (p->useFilePath == filePath)
        return;

    p->fetchFileInfo = true;
    p->fileInfo.setFilePath(filePath);
    p->useFilePath = p->fileInfo.filePath();
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

    if (mode & ReadOnly) {
        mode = p->initRead(mode);
    } else if (mode & WriteOnly) {
        mode = p->initWrite(mode);
    }

    if (mode == NotOpen)
        return false;

    p->clearZipError();
    return QIODevice::open(mode);
}

void QuaZipFile::setPassword(QString *password)
{
    if (isOpen()) {
        qWarning("QuaZipFile::setPassword d");
        return;
    }

    if (!password || password->isNull()) {
        p->fileInfo.setIsEncrypted(false);
        return;
    }

    QuaZipKeysGenerator keyGen(p->zip ? p->zip->passwordCodec() : nullptr);
    keyGen.addPassword(*password);
    p->fileInfo.setCryptKeys(keyGen.keys());
}

bool QuaZipFile::isSequential() const
{
    if (isReadable())
        return p->zip->ioDevice()->isSequential();

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
        return isRaw() ? compressedSize() : uncompressedSize();

    if (isWritable()) {
        return qint64(p->writePos);
    }

    qWarning("QuaZipFile::size(): file is not open");
    return -1;
}

qint64 QuaZipFile::compressedSize() const
{
    if (isWritable()) {
        return qint64(zipTotalCompressedBytes(p->zip->getZipFile()));
    }

    return fileInfo().compressedSize();
}

qint64 QuaZipFile::uncompressedSize() const
{
    auto &fileInfo = this->fileInfo();
    if (isWritable() && !isRaw())
        return qint64(p->writePos);

    return fileInfo.uncompressedSize();
}

const QuaZipFileInfo &QuaZipFile::fileInfo() const
{
    if (!isOpen()) {
        p->initFileInfo();
    }

    return p->fileInfo;
}

void QuaZipFile::close()
{
    if (!isOpen()) {
        qWarning("QuaZipFile::close(): file isn't open");
        return;
    }
    int error = ZIP_OK;
    if (isReadable())
        error = unzCloseCurrentFile(p->zip->getUnzFile());
    else if (isWritable())
        error = zipCloseFileInZip(p->zip->getZipFile());

    if (p->internal) {
        p->zip->close();
        error = p->zip->getZipError();
    }

    p->seekBuffer.clear();
    QIODevice::close();
    if (error != ZIP_OK) {
        p->setZipError(error);
    }
}

qint64 QuaZipFile::readData(char *data, qint64 maxSize)
{
    if (isReadable() && p->seekInternal(pos())) {
        return p->readInternal(data, maxSize);
    }

    return -1;
}

qint64 QuaZipFile::writeData(const char *data, qint64 maxSize)
{
    return p->writeInternal(data, maxSize);
}

QString QuaZipFile::filePath() const
{
    return p->useFilePath;
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

    if (p->caseSensitivity == cs)
        return;

    p->caseSensitivity = cs;
    p->fetchFileInfo = true;
}

bool QuaZipFile::isRaw() const
{
    return fileInfo().isRaw();
}

void QuaZipFile::setIsRaw(bool raw)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change mode when open.");
        return;
    }

    p->fileInfo.setIsRaw(raw);
}

int QuaZipFile::compressionLevel() const
{
    return fileInfo().compressionLevel();
}

void QuaZipFile::setCompressionLevel(int value)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change mode when open.");
        return;
    }

    p->fileInfo.setCompressionLevel(value);
}

int QuaZipFile::compressionMethod() const
{
    return fileInfo().compressionMethod();
}

void QuaZipFile::setCompressionMethod(int value)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change mode when open.");
        return;
    }

    p->fileInfo.setCompressionMethod(quint16(value));
}

int QuaZipFile::compressionStrategy() const
{
    return fileInfo().compressionStrategy();
}

void QuaZipFile::setCompressionStrategy(int value)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change mode when open.");
        return;
    }

    p->fileInfo.setCompressionStrategy(quint16(value));
}

int QuaZipFile::getZipError() const
{
    return p->zipError;
}

const QString &QuaZipFile::comment() const
{
    return fileInfo().comment();
}

void QuaZipFile::setComment(const QString &comment)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change comment when open.");
        return;
    }

    p->fileInfo.setComment(comment);
}

const QuaZExtraField::Map &QuaZipFile::centralExtraFields() const
{
    return fileInfo().centralExtraFields();
}

void QuaZipFile::setCentralExtraFields(const QuaZExtraField::Map &map)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change extra fields when open.");
        return;
    }

    p->fileInfo.setCentralExtraFields(map);
}

const QuaZExtraField::Map &QuaZipFile::localExtraFields() const
{
    return fileInfo().localExtraFields();
}

void QuaZipFile::setLocalExtraFields(const QuaZExtraField::Map &map)
{
    if (isOpen()) {
        qWarning("QuaZipFile cannot change extra fields when open.");
        return;
    }

    p->fileInfo.setLocalExtraFields(map);
}

void QuaZipFilePrivate::clearZipError()
{
    setZipError(UNZ_OK);
}

void QuaZipFilePrivate::setZipError(int zipError)
{
    this->zipError = zipError;
    q->setErrorString(zipError == UNZ_OK
            ? QString()
            : QString("ZIP/UNZIP API error %1").arg(zipError));
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
    , internal(false)
    , fetchFileInfo(true)
    , zipError(UNZ_OK)
{
}

QuaZipFilePrivate::~QuaZipFilePrivate()
{
    if (internal)
        delete zip;
}

bool QuaZipFilePrivate::initFileInfo()
{
    if (fetchFileInfo) {
        if (!useFilePath.isEmpty()) {
            zip->setCurrentFile(useFilePath, caseSensitivity);
            setZipError(zip->getZipError());
            if (zipError != UNZ_OK)
                return false;
        }
        zip->getCurrentFileInfo(fileInfo);
        setZipError(zip->getZipError());
        fetchFileInfo = false;

        if (fileInfo.uncompressedSize() < 0)
            setZipError(UNZ_BADZIPFILE);

        return zipError == UNZ_OK;
    }

    return true;
}

QIODevice::OpenMode QuaZipFilePrivate::initRead(QIODevice::OpenMode mode)
{
    do {
        if (zip == nullptr) {
            zipError = UNZ_PARAMERROR;
            q->setErrorString("Zip archive is not set.");
            break;
        }

        if (internal) {
            Q_ASSERT(!zip->isOpen());
            if (!zip->open(QuaZip::mdUnzip)) {
                setZipError(zip->getZipError());
                break;
            }
        }

        if (zip->getMode() != QuaZip::mdUnzip) {
            zipError = UNZ_PARAMERROR;
            q->setErrorString("Zip archive is not opened for reading.");
            break;
        }

        Q_ASSERT(zip->ioDevice());
        Q_ASSERT(zip->ioDevice()->isReadable());
        Q_ASSERT(!zip->ioDevice()->isTextModeEnabled());

        if (!initFileInfo())
            break;

        if (!zip->hasCurrentFile()) {
            zipError = UNZ_PARAMERROR;
            q->setErrorString("File to read from Zip archive is not found.");
            break;
        }

        setZipError(unzOpenCurrentFile4(zip->getUnzFile(), NULL, NULL,
            int(fileInfo.isRaw()), fileInfo.cryptKeys()));

        if (zipError == UNZ_OK) {
            return mode;
        }
    } while (false);

    return QIODevice::NotOpen;
}

QIODevice::OpenMode QuaZipFilePrivate::initWrite(QIODevice::OpenMode mode)
{
    if (internal) {
        qWarning("QuaZipFile::open(): write mode is incompatible with "
                 "internal QuaZip approach");
        return QIODevice::NotOpen;
    }

    if (zip == nullptr) {
        qWarning("QuaZipFile::open(): zip is NULL");
        return QIODevice::NotOpen;
    }

    switch (zip->getMode()) {
    case QuaZip::mdCreate:
    case QuaZip::mdAppend:
    case QuaZip::mdAdd: {
        Q_ASSERT(zip->ioDevice());
        Q_ASSERT(zip->ioDevice()->isWritable());
        Q_ASSERT(!zip->ioDevice()->isTextModeEnabled());
        QByteArray compatibleFilePath;
        QByteArray compatibleComment;

        zip_fileinfo info_z;
        zip->fillZipInfo(
            info_z, fileInfo, compatibleFilePath, compatibleComment);

        auto resultCode = QuaZExtraField::OK;
        auto centralExtra =
            QuaZExtraField::fromMap(fileInfo.centralExtraFields(), &resultCode);
        switch (resultCode) {
        case QuaZExtraField::ERR_FIELD_SIZE_LIMIT:
        case QuaZExtraField::ERR_BUFFER_SIZE_LIMIT:
            q->setErrorString("Central extra field is too big.");
            zipError = ZIP_PARAMERROR;
            return QIODevice::NotOpen;
        default:
            break;
        }
        auto localExtra =
            QuaZExtraField::fromMap(fileInfo.localExtraFields(), &resultCode);
        switch (resultCode) {
        case QuaZExtraField::ERR_FIELD_SIZE_LIMIT:
        case QuaZExtraField::ERR_BUFFER_SIZE_LIMIT:
            q->setErrorString("Local extra field is too big.");
            zipError = ZIP_PARAMERROR;
            return QIODevice::NotOpen;
        default:
            break;
        }

        info_z.extrafield_global = centralExtra.data();
        info_z.size_extrafield_global = uInt(centralExtra.length());

        info_z.extrafield_global = localExtra.data();
        info_z.size_extrafield_global = uInt(localExtra.length());

        setZipError(zipOpenNewFileInZipKeys(
            zip->getZipFile(), &info_z, fileInfo.cryptKeys()));

        if (zipError == ZIP_OK) {
            writePos = 0;
            if (fileInfo.isText())
                mode |= QIODevice::Text;
            return mode;
        }
        break;
    }

    case QuaZip::mdUnzip:
    case QuaZip::mdNotOpen:
        zipError = ZIP_PARAMERROR;
        q->setErrorString("ZIP file is not writable");
        break;
    }

    return QIODevice::NotOpen;
}

bool QuaZipFilePrivate::seekInternal(qint64 newPos)
{
    if (newPos < 0)
        return false;

    if (zipError != UNZ_OK)
        return false;

    Q_ASSERT(zip);
    Q_ASSERT(zip->getMode() == QuaZip::mdUnzip);
    Q_ASSERT(zip->ioDevice());
    Q_ASSERT(zip->ioDevice()->isReadable());

    if (q->isSequential())
        return true;

    auto uncompressedSize = fileInfo.uncompressedSize();
    if (newPos > uncompressedSize)
        return false;

    auto currentPos = unztell64(zip->getUnzFile());
    if (currentPos > ZPOS64_T(uncompressedSize)) {
        qWarning("Damaged ZIP archive?");
        return false;
    }

    auto skipCount = qint64(currentPos);
    skipCount -= newPos;
    if (skipCount > 0 ||
        currentPos > ZPOS64_T(std::numeric_limits<qint64>::max())) {
        auto unzFile = zip->getUnzFile();
        int err = unzCloseCurrentFile(unzFile);
        if (err == UNZ_OK) {
            err = unzOpenCurrentFile4(unzFile, NULL, NULL,
                int(fileInfo.isRaw()), fileInfo.cryptKeys());
        }

        if (err != UNZ_OK) {
            setZipError(err);
            return false;
        }

        skipCount = newPos;
    } else {
        skipCount = -skipCount;
    }

    int blockSize = SEEK_BUFFER_SIZE;
    QuaZUtils::adjustBlockSize(blockSize, uncompressedSize);

    while (skipCount > 0) {
        QuaZUtils::adjustBlockSize(blockSize, skipCount);
        if (seekBuffer.size() < blockSize) {
            seekBuffer.resize(blockSize);
        }

        auto readBytes = readInternal(seekBuffer.data(), blockSize);
        if (readBytes != blockSize) {
            return false;
        }
        skipCount -= readBytes;
    }

    return true;
}

qint64 QuaZipFilePrivate::readInternal(char *data, qint64 maxlen)
{
    if (zipError != UNZ_OK)
        return -1;

    Q_ASSERT(zip);
    Q_ASSERT(zip->getMode() == QuaZip::mdUnzip);
    Q_ASSERT(zip->ioDevice());
    Q_ASSERT(zip->ioDevice()->isReadable());
    Q_ASSERT(!zip->ioDevice()->isTextModeEnabled());

    if (maxlen <= 0)
        return maxlen;

    qint64 count = maxlen;
    auto blockSize = unsigned(QuaZUtils::maxBlockSize<int>());

    auto uncompressedSize = ZPOS64_T(fileInfo.uncompressedSize());

    auto unzFile = zip->getUnzFile();
    auto currentPos = unztell64(unzFile);
    while (currentPos < uncompressedSize && count > 0) {
        QuaZUtils::adjustBlockSize(blockSize, count);

        if (currentPos + blockSize > uncompressedSize) {
            blockSize = unsigned(uncompressedSize - currentPos);
        }

        int result = unzReadCurrentFile(unzFile, data, blockSize);
        if (result < 0) {
            setZipError(result);
            return -1;
        }

        count -= result;
        currentPos += result;
    }

    return maxlen - count;
}

qint64 QuaZipFilePrivate::writeInternal(const char *data, qint64 maxlen)
{
    if (zipError != ZIP_OK)
        return -1;

    Q_ASSERT(zip);
    Q_ASSERT(zip->isOpen());
    Q_ASSERT(zip->getMode() != QuaZip::mdUnzip);
    Q_ASSERT(zip->ioDevice());
    Q_ASSERT(zip->ioDevice()->isWritable());
    Q_ASSERT(!zip->ioDevice()->isTextModeEnabled());

    if (maxlen <= 0)
        return maxlen;

    qint64 count = maxlen;
    auto blockSize = unsigned(QuaZUtils::maxBlockSize<int>());
    auto zipFile = zip->getZipFile();
    Q_CONSTEXPR auto maxSize = ZPOS64_T(std::numeric_limits<qint64>::max());
    while (writePos < maxSize && count > 0) {
        QuaZUtils::adjustBlockSize(blockSize, count);

        if (writePos + blockSize > maxSize) {
            setZipError(ZIP_INTERNALERROR);
            return -1;
        }

        int result = zipWriteInFileInZip(zipFile, data, blockSize);
        if (result < 0) {
            setZipError(result);
            return -1;
        }

        writePos += blockSize;
        count -= blockSize;
    }

    return maxlen - count;
}
