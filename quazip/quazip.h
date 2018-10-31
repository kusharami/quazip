#ifndef QUA_ZIP_H
#define QUA_ZIP_H

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

#include <QString>
#include <QStringList>
#include <QTextCodec>

#include "quazip_global.h"
#include "quazipfileinfo.h"

// just in case it will be defined in the later versions of the ZIP/UNZIP
#ifndef UNZ_OPENERROR
// define additional error code
#define UNZ_OPENERROR -1000
#endif

/// @cond internal
extern "C" {
struct zlib_filefunc_def_s;
struct zip_fileinfo_s;
}

class QuaZipPrivate;
class QuaZipFile;
class QuaZipFilePrivate;
/// @endcond internal

/// ZIP archive.
/** \class QuaZip quazip.h <quazip/quazip.h>
 * This class implements basic interface to the ZIP archive. It can be
 * used to read table contents of the ZIP archive and retreiving
 * information about the files inside it.
 *
 * You can also use this class to open files inside archive by passing
 * pointer to the instance of this class to the constructor of the
 * QuaZipFile class. But see QuaZipFile::QuaZipFile(QuaZip*, QObject*)
 * for the possible pitfalls.
 *
 * This class is indended to provide interface to the ZIP subpackage of
 * the ZIP/UNZIP package as well as to the UNZIP subpackage. But
 * currently it supports only UNZIP.
 *
 * The use of this class is simple - just create instance using
 * constructor, then set ZIP archive file name using setFile() function
 * (if you did not passed the name to the constructor), then open() and
 * then use different functions to work with it! Well, if you are
 * paranoid, you may also wish to call close before destructing the
 * instance, to check for errors on close.
 *
 * You may also use getUnzFile() and getZipFile() functions to get the
 * ZIP archive handle and use it with ZIP/UNZIP package API directly.
 *
 * This class supports localized file names inside ZIP archive, but you
 * have to set up proper codec with setCodec() function. By default,
 * locale codec will be used, which is probably ok for UNIX systems, but
 * will almost certainly fail with ZIP archives created in Windows. This
 * is because Windows ZIP programs have strange habit of using DOS
 * encoding for file names in ZIP archives. For example, ZIP archive
 * with cyrillic names created in Windows will have file names in \c
 * IBM866 encoding instead of \c WINDOWS-1251. I think that calling one
 * function is not much trouble, but for true platform independency it
 * would be nice to have some mechanism for file name encoding auto
 * detection using locale information. Does anyone know a good way to do
 * it?
 **/
class QUAZIP_EXPORT QuaZip {
    /// @cond internal
    friend class QuaZipPrivate;
    /// @endcond

public:
    /// Open mode of the ZIP file.
    enum Mode
    {
        mdNotOpen, ///< ZIP file is not open. This is the initial mode.
        mdUnzip, ///< ZIP file is open for reading files inside it.
        mdCreate, ///< ZIP file was created with open() call.
        mdAppend, /**< ZIP file was opened in append mode. This refers to
                  * \c APPEND_STATUS_CREATEAFTER mode in ZIP/UNZIP package
                  * and means that zip is appended to some existing file
                  * what is useful when that file contains
                  * self-extractor code. This is obviously \em not what
                  * you whant to use to add files to the existing ZIP
                  * archive.
                  **/
        mdAdd ///< ZIP file was opened for adding files in the archive.
    };
    /// Case sensitivity for the file names.
    /** This is what you specify when accessing files in the archive.
     * Works perfectly fine with any characters thanks to Qt's great
     * unicode support. This is different from ZIP/UNZIP API, where
     * only US-ASCII characters was supported.
     **/
    enum CaseSensitivity
    {
        /// Default for platform. Case sensitive for UNIX, not for Windows/MacOS.
        csDefault = 0,
        /// Case sensitive file names
        csSensitive = 1,
        /// Case insensitive file names
        csInsensitive = 2
    };
    /// Compatibility flags
    enum CompatibilityFlag
    {
        /** When no system compatibility is set, filePathCodec() and
         * commentCodec() will be used to decode and encode file names
         * and comments
         * \note Other flags will override this one
          */
        CustomCompatibility = 0,
        /** On Windows file names and comments will be stored in current
         * OEM code page if it is not UTF-7 or UTF-8.
         * In other cases and on other systems IBM 437, or IBM 850,
         * or IBM 866 code page will be used.
         *
         * File names will be stored in 8.3 mode
         * (8 characters for name, 3 characters for extension)
         */
        DosCompatible = 0x01,
        /** When non-ASCII characters used, file names and comments are stored
         * in UTF-8 and Unicode flag is set.
         *
         * If combined with DOS_Compatible, non-ASCII file names are encoded
         * with UTF-8 and stored in Extra Field 0x7075 (Info-ZIP Unicode Path).
         * Non-ASCII comments are stored in
         * Extra Field 0x6375 (Info-ZIP Unicode Comment)
         */
        UnixCompatible = 0x02,
        /** When non-ASCII characters used, file names and comments are stored
         * in UTF-8 and Unicode flag is set.
         *
         * If combined with DOS_Compatible, non-ASCII file names and
         * comments are encoded in UTF-8 and stored in
         * ZipArchive Extra Field 0x5A4C
         */
        WindowsCompatible = 0x04,
        /// By default Unix and Windows compatibility is ON.
        DefaultCompatibility = UnixCompatible | WindowsCompatible
    };
    Q_DECLARE_FLAGS(CompatibilityFlags, CompatibilityFlag)

    /// Returns the actual case sensitivity for the specified QuaZIP one.
    /**
      \param cs The value to convert.
      \returns If CaseSensitivity::csDefault, then returns the default
      file name case sensitivity for the platform. Otherwise, just
      returns the appropriate value from the Qt::CaseSensitivity enum.
      */
    static Qt::CaseSensitivity convertCaseSensitivity(CaseSensitivity cs);

private:
    QuaZipPrivate *p;
    // not (and will not be) implemented
    QuaZip(const QuaZip &that);
    // not (and will not be) implemented
    QuaZip &operator=(const QuaZip &that);

public:
    /// Constructs QuaZip object.
    /** Call setName() before opening constructed object. */
    QuaZip();
    /// Constructs QuaZip object associated with ZIP file \a zipName.
    QuaZip(const QString &zipName);
    /// Constructs QuaZip object associated with ZIP file represented by \a ioDevice.
    /** The IO device must be seekable, otherwise an error will occur when opening. */
    QuaZip(QIODevice *ioDevice);
    /// Destroys QuaZip object.
    /** Calls close() if necessary. */
    ~QuaZip();
    /// Opens ZIP file.
    /**
     * Argument \a mode specifies open mode of the ZIP archive. See Mode
     * for details.
     *
     * If the ZIP file is accessed via explicitly set QIODevice, then
     * this device is opened in the necessary mode. If the device was
     * already opened by some other means, then QuaZIP checks if the
     * open mode is compatible to the mode needed for the requested operation.
     * If necessary, seeking is performed to position the device properly.
     *
     * \return \c true if successful, \c false otherwise.
     *
     * \note ZIP/UNZIP API open calls do not return error code - they
     * just return \c NULL indicating an error. But to make things
     * easier, quazip.h header defines additional error code \c
     * UNZ_ERROROPEN and getZipError() will return it if the open call
     * of the ZIP/UNZIP API returns \c NULL.
     **/
    bool open(Mode mode);
    /// Closes ZIP file.
    /** Call getZipError() to determine if the close was successful.
     *
     * If the file was opened by name, then the underlying QIODevice is closed
     * and deleted.
     *
     * If the underlying QIODevice was set explicitly using setIoDevice() or
     * the appropriate constructor, then it is closed if the auto-close flag
     * is set (which it is by default). Call setAutoClose() to clear the
     * auto-close flag if this behavior is undesirable.
     *
     * \note When using QSaveFile you should call setAutoClose(false),
     * because it will crash when close called.
      */
    void close();
    /// Sets the codec used to encode/decode file names inside archive.
    /** \note This codec is used only when
     * compatibilityFlags() equals to CustomCompatibility.
     **/
    void setFilePathCodec(QTextCodec *filePathCodec);
    /// Sets the codec used to encode/decode file names inside archive.
    /** \overload
     * Equivalent to calling setFilePathCodec(QTextCodec::codecForName(codecName));
     **/
    void setFilePathCodec(const char *filePathCodecName);
    /// Codec to be used to encode/decode comments inside archive when
    /// compatibilityFlags() equals to CustomCompatibility.
    /**
     * \note On Windows This codec defaults to current OEM code page
     * if it is not UTF-7 or UTF-8.
     * In other cases and on other systems it defaults to IBM 437,
     * or IBM 850, or IBM 866 code page.
     */
    QTextCodec *filePathCodec() const;
    /// Sets the codec used to encode/decode comments inside archive.
    /** \note This codec is used only when
     * compatibilityFlags() equals to CustomCompatibility.
     *
     * When writing a global comment with Non-ASCII characters it is encoded
     * with UTF-8 and stored with BOM header
     */
    void setCommentCodec(QTextCodec *commentCodec);
    /// Sets the codec used to encode/decode comments inside archive.
    /** \overload
     * Equivalent to calling setCommentCodec(QTextCodec::codecForName(codecName));
     **/
    void setCommentCodec(const char *commentCodecName);
    /// Codec to be used to encode/decode comments inside archive when
    /// compatibilityFlags() equals to CustomCompatibility.
    /**
    * \note This codec defaults to current locale text codec
    */
    QTextCodec *commentCodec() const;

    QTextCodec *passwordCodec() const;
    void setPasswordCodec(QTextCodec *codec);
    void setPasswordCodec(const char *codecName);
    /// Returns the name of the ZIP file.
    /** Returns null string if no ZIP file name has been set, for
     * example when the QuaZip instance is set up to use a QIODevice
     * instead.
     * \sa setZipFilePath(), setIoDevice(), getIODevice()
     **/
    QString zipFilePath() const;
    /// Sets the name of the ZIP file.
    /** Does nothing if the ZIP file is open.
     *
     * Does not reset error code returned by getZipError().
     * \sa setIoDevice(), getIODevice(), zipFilePath()
     **/
    void setZipFilePath(const QString &zipName);
    /// Returns the device representing this ZIP file.
    /**
     * \sa setIoDevice(), zipFilePath(), setZipFilePath()
     **/
    QIODevice *ioDevice() const;
    /// Sets the device representing the ZIP file.
    /** Does nothing if the ZIP file is open.
     *
     * Does not reset error code returned by getZipError().
     * \sa getIODevice(), zipFilePath(), setZipFilePath()
     **/
    void setIODevice(QIODevice *ioDevice);
    /// Returns the mode in which ZIP file was opened.
    Mode getMode() const;
    /// Returns \c true if ZIP file is open, \c false otherwise.
    bool isOpen() const;
    /// Returns the error code of the last operation.
    /** Returns \c UNZ_OK if the last operation was successful.
     *
     * Error code resets to \c UNZ_OK every time you call any function
     * that accesses something inside ZIP archive, even if it is \c
     * const (like getEntriesCount()). open() and close() calls reset
     * error code too. See documentation for the specific functions for
     * details on error detection.
     **/
    int getZipError() const;
    /// Returns number of the entries in the ZIP central directory.
    /** Returns negative error code in the case of error. The same error
     * code will be returned by subsequent getZipError() call.
     **/
    int getEntriesCount() const;
    /// Returns global comment in the ZIP file.
    /// \note Comment is decoded with commentCodec() when there is no
    /// UTF BOM Header present, otherwise UTF codec is used,
    QString getComment() const;
    /// Sets the global comment in the ZIP file.
    /** The comment will be written to the archive on close operation.
     * QuaZip makes a distinction between a null QString() comment
     * and an empty &quot;&quot; comment in the QuaZip::mdAdd mode.
     * A null comment is the default and it means &quot;don't change
     * the comment&quot;. An empty comment removes the original comment.
     *
     * \note When there are non-ASCII characters, the comment is stored
     * in UTF-8 encoding with BOM
     *
     * \sa open(), setCommentCodec()
     **/
    void setComment(const QString &comment);
    /// Sets the current file to the first file in the archive.
    /** Returns \c true on success, \c false otherwise. Call
     * getZipError() to get the error code.
     **/
    bool goToFirstFile();
    /// Sets the current file to the next file in the archive.
    /** Returns \c true on success, \c false otherwise. Call
     * getZipError() to determine if there was an error.
     *
     * Should be used only in QuaZip::mdUnzip mode.
     *
     * \note If the end of file was reached, getZipError() will return
     * \c UNZ_OK instead of \c UNZ_END_OF_LIST_OF_FILE. This is to make
     * things like this easier:
     * \code
     * for(bool more=zip.goToFirstFile(); more; more=zip.goToNextFile()) {
     *   // do something
     * }
     * if(zip.getZipError()==UNZ_OK) {
     *   // ok, there was no error
     * }
     * \endcode
     **/
    bool goToNextFile();
    /// Sets current file by its name.
    /** Returns \c true if successful, \c false otherwise. Argument \a
     * cs specifies case sensitivity of the file name. Call
     * getZipError() in the case of a failure to get error code.
     *
     * This is not a wrapper to unzLocateFile() function. That is
     * because I had to implement locale-specific case-insensitive
     * comparison.
     *
     * Here are the differences from the original implementation:
     *
     * - If the file was not found, error code is \c UNZ_OK, not \c
     *   UNZ_END_OF_LIST_OF_FILE (see also goToNextFile()).
     * - If this function fails, it unsets the current file rather than
     *   resetting it back to what it was before the call.
     *
     * If \a fileName is null string then this function unsets the
     * current file and return \c true. Note that you should close the
     * file first if it is open! See
     * QuaZipFile::QuaZipFile(QuaZip*,QObject*) for the details.
     *
     * Should be used only in QuaZip::mdUnzip mode.
     *
     * \sa setFilePathCodec(), CaseSensitivity
     **/
    bool setCurrentFile(
        const QString &fileName, CaseSensitivity cs = csDefault);
    /// Returns \c true if the current file has been set.
    bool hasCurrentFile() const;
    /// Retrieves information about the current file.
    /** Fills the structure pointed by \a info. Returns \c true on
     * success, \c false otherwise. In the latter case structure pointed
     * by \a info remains untouched. If there was an error,
     * getZipError() returns error code.
     *
     * Should be used only in QuaZip::mdUnzip mode.
     *
     * Does nothing and returns \c false in any of the following cases.
     * - ZIP is not open;
     * - ZIP does not have current file.
     *
     * In both cases getZipError() returns \c UNZ_OK since there
     * is no ZIP/UNZIP API call.
     *
     **/
    bool getCurrentFileInfo(QuaZipFileInfo &info) const;
    /// Returns the current file name.
    /** Equivalent to calling getCurrentFileInfo() and then getting \c
     * name field of the QuaZipFileInfo structure, but faster and more
     * convenient.
     *
     * Should be used only in QuaZip::mdUnzip mode.
     **/
    QString currentFilePath() const;
    /// Changes the data descriptor writing mode.
    /**
      According to the ZIP format specification, a file inside archive
      may have a data descriptor immediately following the file
      data. This is reflected by a special flag in the local file header
      and in the central directory. By default, QuaZIP sets this flag
      and writes the data descriptor unless both method and level were
      set to 0, in which case it operates in 1.0-compatible mode and
      never writes data descriptors.

      By setting this flag to false, it is possible to disable data
      descriptor writing, thus increasing compatibility with archive
      readers that don't understand this feature of the ZIP file format.

      Setting this flag affects all the QuaZipFile instances that are
      opened after this flag is set.

      The data descriptor writing mode is enabled by default.

      Note that if the ZIP archive is written into a QIODevice for which
      QIODevice::isSequential() returns \c true, then the data descriptor
      is mandatory and will be written even if this flag is set to false.

      \param enabled If \c true, enable local descriptor writing,
      disable it otherwise.

      \sa QuaZipFile::isDataDescriptorWritingEnabled()
      */
    void setDataDescriptorWritingEnabled(bool enabled);
    /// Returns the data descriptor default writing mode.
    /**
      \sa setDataDescriptorWritingEnabled()
      */
    bool isDataDescriptorWritingEnabled() const;
    /// Returns a list of files inside the archive.
    /**
      \return A list of file names or an empty list if there
      was an error or if the archive is empty (call getZipError() to
      figure out which).
      \sa getFileInfoList()
      */
    QStringList getFileNameList() const;
    /// Returns information list about all files inside the archive.
    /**
      \return A list of QuaZipFileInfo objects or an empty list if there
      was an error or if the archive is empty (call getZipError() to
      figure out which).
      */
    QList<QuaZipFileInfo> getFileInfoList() const;

    /// Compatibility flags for next files to be compressed.
    /// \sa CompatibilityFlags
    CompatibilityFlags compatibilityFlags() const;
    /// Set compatibility flags for next files to be compressed.
    /**
     * @param flags Compatibility flags combined together with logical OR.
     * \sa CompatibilityFlags
     */
    void setCompatibilityFlags(CompatibilityFlags flags);

    /// Enables the zip64 mode.
    /**
     * @param zip64 If \c true, the zip64 mode is enabled, disabled otherwise.
     *
     * Once this is enabled, all new files (until the mode is disabled again)
     * will be created in the zip64 mode, thus enabling the ability to write
     * files larger than 4 GB. By default, the zip64 mode is off due to
     * compatibility reasons.
     *
     * Note that this does not affect the ability to read zip64 archives in any
     * way.
     *
     * \sa isZip64Enabled()
     */
    void setZip64Enabled(bool zip64);
    /// Returns whether the zip64 mode is enabled.
    /**
     * @return \c true if and only if the zip64 mode is enabled.
     *
     * \sa setZip64Enabled()
     */
    bool isZip64Enabled() const;
    /// Returns the auto-close flag.
    /**
      @sa setAutoClose()
      */
    bool isAutoClose() const;
    /// Sets or unsets the auto-close flag.
    /**
      By default, QuaZIP opens the underlying QIODevice when open() is called,
      and closes it when close() is called. In some cases, when the device
      is set explicitly using setIoDevice(), it may be desirable to
      leave the device open. If the auto-close flag is unset using this method,
      then the device isn't closed automatically if it was set explicitly.

      If it is needed to clear this flag, it is recommended to do so before
      opening the archive because otherwise QuaZIP may close the device
      during the open() call if an error is encountered after the device
      is opened.

      If the device was not set explicitly, but rather the setZipFilePath() or
      the appropriate constructor was used to set the ZIP file name instead,
      then the auto-close flag has no effect, and the internal device
      is closed nevertheless because there is no other way to close it.

      \note For QSaveFile should be set to false,
        otherwise will crash on close().

      @sa isAutoClose()
      @sa setIoDevice()
      */
    void setAutoClose(bool autoClose) const;
    /// Returns default file path codec
    static QTextCodec *defaultFilePathCodec();
    /// Sets the default file name codec to use.
    /**
     * The default codec is used by the constructors, so calling this function
     * won't affect the QuaZip instances already created at that moment.
     *
     * The codec specified here can be overriden by calling setFilePathCodec().
     * If neither function is called, QuaZipTextCodec will be used
     * to decode or encode file names. Use this function with caution if
     * the application uses other libraries that depend on QuaZip. Those
     * libraries can either call this function by themselves, thus overriding
     * your setting or can rely on the default encoding, thus failing
     * mysteriously if you change it. For these reasons, it isn't recommended
     * to use this function if you are developing a library, not an application.
     * Instead, ask your library users to call it in case they need specific
     * encoding.
     *
     * In most cases, using setFilePathCodec() instead is the right choice.
     * However, if you depend on third-party code that uses QuaZip, then the
     * reasons stated above can actually become a reason to use this function
     * in case the third-party code in question fails because it doesn't
     * understand the encoding you need and doesn't provide a way to specify it.
     * This applies to the JlCompress class as well, as it was contributed and
     * doesn't support explicit encoding parameters.
     *
     * In short: use setFilePathCodec() when you can, resort to
     * setDefaultFilePathCodec() when you don't have access to the QuaZip
     * instance.
     *
     * @param codec The codec to use by default. If NULL, resets to default.
     */
    static void setDefaultFilePathCodec(QTextCodec *codec);
    /// Returns default comment codec
    static QTextCodec *defaultCommentCodec();
    /// Sets the default comment codec to use.
    /**
     * The default codec is used by the constructors, so calling this function
     * won't affect the QuaZip instances already created at that moment.
     *
     * The codec specified here can be overriden by calling setCommentCodec().
     * If neither function is called, QTextCodec::codecForLocale() will be used
     * to decode and encode comments.
     */
    static void setDefaultCommentCodec(QTextCodec *codec);
    /**
     * @overload
     * Equivalent to calling
     * setDefaultFilePathCodec(QTextCodec::codecForName(codecName)).
     */
    static void setDefaultFilePathCodec(const char *codecName);
    /**
     * @overload
     * Equivalent to calling
     * setDefaultCommentCodec(QTextCodec::codecForName(codecName)).
     */
    static void setDefaultCommentCodec(const char *codecName);
    /// Returns default password codec.
    /// \note Same as QuaZipKeysGenerator::defaultPasswordCodec()
    static QTextCodec *defaultPasswordCodec();
    /// Sets default password codec.
    /// \note Same as QuaZipKeysGenerator::setDefaultPasswordCodec()
    static void setDefaultPasswordCodec(QTextCodec *codec = nullptr);

    /// QuaZip constructor will set this flags instead of DefaultCompatibility.
    /// \sa compatibilityFlags(), setDefaultFilePathCodec(),
    ///  setDefaultCommentCodec()
    static void setDefaultCompatibilityFlags(CompatibilityFlags flags);
    static CompatibilityFlags defaultCompatibilityFlags();

private:
    friend class QuaZipFile;
    friend class QuaZipFilePrivate;
    void fillZipInfo(zip_fileinfo_s &zipInfo, QuaZipFileInfo &fileInfo,
        QByteArray &compatibleFilePath, QByteArray &compatibleComment) const;
    void *getUnzFile() const;
    void *getZipFile() const;
};

#endif
