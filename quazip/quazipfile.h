#ifndef QUA_ZIPFILE_H
#define QUA_ZIPFILE_H

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

#include <QIODevice>

#include "quazipfileinfo.h"
#include "quazextrafield.h"
#include "quazip.h"

/// A file inside ZIP archive.
/** \class QuaZipFile quazipfile.h <quazip/quazipfile.h>
 * This is the most interesting class. Not only it provides C++
 * interface to the ZIP/UNZIP package, but also integrates it with Qt by
 * subclassing QIODevice. This makes possible to access files inside ZIP
 * archive using QTextStream or QDataStream, for example. Actually, this
 * is the main purpose of the whole QuaZIP library.
 *
 * You can either use existing QuaZip instance to create instance of
 * this class or pass ZIP archive file name to this class, in which case
 * it will create internal QuaZip object. See constructors' descriptions
 * for details. Writing is only possible with the existing instance.
 *
 * Note that due to the underlying library's limitation it is not
 * possible to use multiple QuaZipFile instances to open several files
 * in the same archive at the same time. If you need to write to
 * multiple files in parallel, then you should write to temporary files
 * first, then pack them all at once when you have finished writing. If
 * you need to read multiple files inside the same archive in parallel,
 * you should extract them all into a temporary directory first.
 *
 * \section quazipfile-sequential Sequential or random-access?
 *
 * At the first thought, QuaZipFile has fixed size, the start and the
 * end and should be therefore considered random-access device. But
 * there is one major obstacle to making it random-access: ZIP/UNZIP API
 * does not support seek() operation and the only way to implement it is
 * through reopening the file and re-reading to the required position,
 * but this is prohibitively slow.
 *
 * Therefore, QuaZipFile is considered to be a sequential device. This
 * has advantage of availability of the ungetChar() operation (QIODevice
 * does not implement it properly for non-sequential devices unless they
 * support seek()). Disadvantage is a somewhat strange behaviour of the
 * size() and pos() functions. This should be kept in mind while using
 * this class.
 *
 **/

/// @cond internal
class QuaZipFilePrivate;
/// @endcond internal
class QUAZIP_EXPORT QuaZipFile : public QIODevice {
    Q_OBJECT

private:
    QuaZipFilePrivate *p;
    friend class QuaZipFilePrivate;

protected:
    /// Implementation of the QIODevice::readData().
    virtual qint64 readData(char *data, qint64 maxSize) override;
    /// Implementation of the QIODevice::writeData().
    virtual qint64 writeData(const char *data, qint64 maxSize) override;

public:
    /// Constructs a QuaZipFile instance.
    /** You should use setZipFilePath() and setFileName() or setZip() before
     * trying to call open() on the constructed object.
     **/
    QuaZipFile();
    /// Constructs a QuaZipFile instance.
    /** \a parent argument specifies this object's parent object.
     *
     * You should use setZipFilePath() and setFileName() or setZip() before
     * trying to call open() on the constructed object.
     **/
    QuaZipFile(QObject *parent);
    /// Constructs a QuaZipFile instance.
    /** \a parent argument specifies this object's parent object and \a
     * zipFilePath specifies ZIP archive file path.
     *
     * You should use setFileName() before trying to call open() on the
     * constructed object.
     **/
    QuaZipFile(const QString &zipFilePath, QObject *parent = NULL);
    /// Constructs a QuaZipFile instance.
    /** \a parent argument specifies this object's parent object, \a
     * zipFilePath specifies ZIP archive file path and \a filePath and \a cs
     * specify a path of the file to open inside archive.
     *
     * \sa setFilePath(), QuaZip::setCurrentFile()
     **/
    QuaZipFile(const QString &zipFilePath, const QString &filePath,
        QuaZip::CaseSensitivity cs = QuaZip::csDefault, QObject *parent = NULL);
    /// Constructs a QuaZipFile instance.
    /** \a parent argument specifies this object's parent object.
     *
     * \a zip is the pointer to the existing QuaZip object. This
     * QuaZipFile object then can be used to read current file in the
     * \a zip or to write to the file inside it.
     *
     * \warning Using this constructor for reading current file can be
     * tricky. Let's take the following example:
     * \code
     * QuaZip zip("archive.zip");
     * zip.open(QuaZip::mdUnzip);
     * zip.setCurrentFile("file-in-archive");
     * QuaZipFile file(&zip);
     * file.open(QIODevice::ReadOnly);
     * // ok, now we can read from the file
     * file.read(somewhere, some);
     * zip.setCurrentFile("another-file-in-archive"); // oops...
     * QuaZipFile anotherFile(&zip);
     * anotherFile.open(QIODevice::ReadOnly);
     * anotherFile.read(somewhere, some); // this is still ok...
     * file.read(somewhere, some); // and this is NOT
     * \endcode
     * So, what exactly happens here? When we change current file in the
     * \c zip archive, \c file that references it becomes invalid
     * (actually, as far as I understand ZIP/UNZIP sources, it becomes
     * closed, but QuaZipFile has no means to detect it).
     *
     * Summary: do not close \c zip object or change its current file as
     * long as QuaZipFile is open. Even better - use another constructors
     * which create internal QuaZip instances, one per object, and
     * therefore do not cause unnecessary trouble. This constructor may
     * be useful, though, if you already have a QuaZip instance and do
     * not want to access several files at once. Good example:
     * \code
     * QuaZip zip("archive.zip");
     * zip.open(QuaZip::mdUnzip);
     * // first, we need some information about archive itself
     * QByteArray comment=zip.getComment();
     * // and now we are going to access files inside it
     * QuaZipFile file(&zip);
     * for(bool more=zip.goToFirstFile(); more; more=zip.goToNextFile()) {
     *   file.open(QIODevice::ReadOnly);
     *   // do something cool with file here
     *   file.close(); // do not forget to close!
     * }
     * zip.close();
     * \endcode
     **/
    QuaZipFile(QuaZip *zip, QObject *parent = NULL);
    /// Destroys a QuaZipFile instance.
    /** Closes file if open, destructs internal QuaZip object (if it
     * exists and \em is internal, of course).
     **/
    virtual ~QuaZipFile() override;
    /// Returns the ZIP archive file path.
    /** If this object was created by passing QuaZip pointer to the
     * constructor, this function will return that QuaZip's file name
     * (or null string if that object does not have file name yet).
     *
     * Otherwise, returns associated ZIP archive file name or null
     * string if there are no name set yet.
     *
     * \sa setZipFilePath() getFileName()
     **/
    QString zipFilePath() const;
    /// Sets the ZIP archive file name.
    /** Automatically creates internal QuaZip object and destroys
     * previously created internal QuaZip object, if any.
     *
     * Will do nothing if this file is already open. You must close() it
     * first.
     **/
    void setZipFilePath(const QString &zipFilePath);
    /// Returns a pointer to the associated QuaZip object.
    /** Returns \c NULL if there is no associated QuaZip or it is
     * internal (so you will not mess with it).
     **/
    QuaZip *zip() const;
    /// Binds to the existing QuaZip instance.
    /** This function destroys internal QuaZip object, if any, and makes
     * this QuaZipFile to use current file in the \a zip object for any
     * further operations. See QuaZipFile(QuaZip*,QObject*) for the
     * possible pitfalls.
     *
     * Will do nothing if the file is currently open. You must close()
     * it first.
     **/
    void setZip(QuaZip *zip);

    /// Returns file info stored or to be stored in the archive
    /**
     * \note If no filePath() was set this function does the same as calling
     * QuaZip::getCurrentFileInfo() on the associated QuaZip object,
     * but you can not call getCurrentFileInfo() if the associated
     * QuaZip is internal (because you do not have access to it), while
     * you still can call this function in that case.
     **/
    const QuaZipFileInfo &fileInfo() const;
    /// Set file info to be stored in the archive
    /// \note If you open the device for reading, this data will be overwritten
    /// with information associated with the file in archive
    void setFileInfo(const QuaZipFileInfo &info);

    /// Returns file path that will be stored or searched in archive.
    /** This function returns file pathed you passed to this object either
     * by using
     * QuaZipFile(const QString&,const QString&,QuaZip::CaseSensitivity,QObject*)
     * or by calling setFileName(). Real name of the file may differ in
     * case if you used case-insensitivity.
     *
     * Returns null string if there is no file name set yet. This is the
     * case when this QuaZipFile operates on the existing QuaZip object
     * (constructor QuaZipFile(QuaZip*,QObject*) or setZip() was used).
     *
     * \sa actualFilePath
     **/
    QString filePath() const;
    /// Returns case sensitivity of the file path.
    /** This function returns case sensitivity argument you passed to
     * this object either by using
     * QuaZipFile(const QString&,const QString&,QuaZip::CaseSensitivity,QObject*)
     * or by calling setFileName().
     *
     * Returns unpredictable value if getFileName() returns null string
     * (this is the case when you did not used setFileName() or
     * constructor above).
     *
     * \sa getFileName
     **/
    /// Sets the file path.
    /**
     * \param filePath File path in Zip archive.
     * If empty, then QuaZip::getCurrentFile() will be used
     *
     * \sa QuaZip::setCurrentFile()
     **/
    void setFilePath(const QString &filePath);

    /// Case sensitivity of filePath to search in archive
    QuaZip::CaseSensitivity caseSensitivity() const;
    /// Changes case sensitivity to search a file in zip archive
    /// \sa setFileName
    void setCaseSensitivity(QuaZip::CaseSensitivity cs);
    /// Returns the actual file path in the archive.
    /** This is \em not a ZIP archive file name, but a name of file inside
     * archive. It is not necessary the same name that you have passed
     * to the
     * QuaZipFile(const QString&,const QString&,QuaZip::CaseSensitivity,QObject*),
     * setFileName() or QuaZip::setCurrentFile() - this is the real file
     * name inside archive, so it may differ in case if the file name
     * search was case-insensitive.
     *
     * Equivalent to calling currentFilePath() on the associated
     * QuaZip object. Returns null string if there is no associated
     * QuaZip object or if it does not have a current file yet. And this
     * is the case if you called setFileName() but did not open the
     * file yet. So this is perfectly fine:
     * \code
     * QuaZipFile file("somezip.zip");
     * file.setFileName("somefile");
     * QString name=file.getName(); // name=="somefile"
     * QString actual=file.actualFilePath(); // actual is null string
     * file.open(QIODevice::ReadOnly);
     * QString actual=file.actualFilePath(); // actual can be "SoMeFiLe" on Windows
     * \endcode
     *
     * \sa zipFilePath(), getFileName(), QuaZip::CaseSensitivity
     **/
    QString actualFilePath() const;

    /// Opens a file for reading or writing.
    /** Returns \c true on success, \c false otherwise.
     * \param mode can be QIODevice::ReadOnly or QIODevice::WriteOnly.
     * May be combined with QIODevice::Text. If you want to read text.
     *
     * \note fileInfo().isText() is set then QIODevice::Text is automatically on
     * \note QIODevice::Unbuffered will be added to \a mode
     * \note QIODevice::Truncate will be added if mode is QIODevice::WriteOnly
     * Call getZipError() to get error code.
     **/
    virtual bool open(OpenMode mode) override;

    /// Set the password to use for file data encryption/decryption
    /**
     * \param password Password will be encoded with QuaZip::passwordCodec()
     * It is removed from memory after this call for security reasons
     *
     * \note Nothing is changed when QuaZipFile is already open
     * \sa open()
     **/
    void setPassword(QString *password);

    /// If we want to read raw data from the file without decompression,
    /// or to write already compressed data
    bool isRaw() const;
    /// Set if we want to read raw data from the file without decompression,
    /// or to write already compressed data
    /// \note Nothing is changed when QuaZipFile is already open
    void setIsRaw(bool raw);

    /// Compression level.
    /// Default is Z_DEFAULT_COMPRESSION
    int compressionLevel() const;
    /// Set compression level
    /**
      \param value The compression level (Z_BEST_COMPRESSION etc.),
    */
    /// \note Nothing is changed when QuaZipFile is already open
    void setCompressionLevel(int value);

    /// Compression method.
    /// Default is Z_DEFLATED
    int compressionMethod() const;
    /// Set compression method
    /// \param value Only Z_NO_COMPRESSION and Z_DEFLATED is supported
    /// \note Nothing is changed when QuaZipFile is already open
    void setCompressionMethod(int value);

    /// Compression strategy.
    /// Default is Z_DEFAULT_STRATEGY
    int compressionStrategy() const;
    /// Set compression strategy
    /**
      \param value The compression strategy:
        Z_DEFAULT_STRATEGY, Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED
    */
    /// \note Nothing is changed when QuaZipFile is already open
    void setCompressionStrategy(int value);

    /// Returns \c true in write mode
    /// Returns \c true in read mode when ZIP archive is sequential.
    /// Returns \c false in read mode when ZIP archive is not sequential.
    virtual bool isSequential() const override;
    /// Returns true if the end of the compressed stream is reached.
    virtual bool atEnd() const override;
    /// Returns the number of the bytes buffered.
    virtual qint64 bytesAvailable() const override;
    /// Returns file size.
    /** Returns uncompressed size if the file is open for reading.
     * Returns number of uncompressed bytes written if it is open for writing.
     * Returns compressed size if isRaw().
     * Returns -1 on error, call getZipError() to get error code.
     * \sa uncompressedSize(), compressedSize()
     **/
    virtual qint64 size() const override;
    /// Returns compressed file size.
    /**
     * \note When open for reading returns compressed size stored in archive.
     * \note When open for writing returns how much compressed bytes
     * was written in archive so far
     * Returns -1 on error, call getZipError() to get error code.
     **/
    qint64 compressedSize() const;
    /// Returns uncompressed file size.
    /**
     * \note When open for reading returns uncompressed size stored in archive.
     * \noteWhen open for writing returns how much original data
     * was compressed so far
     * Returns -1 on error, call getZipError() to get error code.
     **/
    qint64 uncompressedSize() const;

    /// Returns the time when original file was created
    QDateTime creationTime() const;
    /// Set the time when original file was created
    /// \note Will be overwritten when open for reading
    void setCreationTime(const QDateTime &time);

    /// Returns the last time when original file was modified
    QDateTime modificationTime() const;
    /// Set the last time when original file was modified
    /// \note Will be overwritten when open for reading
    void setModificationTime(const QDateTime &time);

    /// Returns the last time when original file was accessed
    QDateTime lastAccessTime() const;
    /// Set the last time when original file was accessed
    /// \note Will be overwritten when open for reading
    void setLastAccessTime(const QDateTime &time);

    /// Closes the file.
    /** Call getZipError() to determine if the close was successful.
     **/
    virtual void close() override;

    /// Returns the error code returned by the last ZIP/UNZIP API call.
    int getZipError() const;

    const QString &comment() const;
    void setComment(const QString &comment);

    const QuaZExtraField::Map &extraFields() const;
    void setExtraFields(const QuaZExtraField::Map &map);
};

#endif
