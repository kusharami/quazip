#ifndef QUA_ZIPNEWINFO_H
#define QUA_ZIPNEWINFO_H

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

#include <QDateTime>
#include <QFile>
#include <QString>

#include "quazip_global.h"

#include "quazipfileinfo.h"

/// Information about a file to be created.
/** This structure holds information about a file to be created inside
 * ZIP archive. At least name should be set to something correct before
 * passing this structure to
 * QuaZipFile::open(OpenMode,const QuaZipNewInfo&,int,int,bool).
 *
 **/
struct QUAZIP_EXPORT QuaZipNewInfo {
    enum EntryType
    {
        /// This is a file
        File,
        /// This is a directory
        Directory,
        /// This is a symbolic link to another file or directory
        SymLink
    };

    /// File name.
    /** This field holds file name inside archive, including path relative
     * to archive root.
     **/
    QString name;
    /// Time when original file was modified
    /// Write in local file header and NTFS extra field
    QDateTime modificationTime;
    /// Permissions to access the file
    QFile::Permissions permissions;
    /// What is it: a File, Directory or a Symbolic Link
    EntryType entryType;
    /** This is only needed if you are using raw file zipping mode, i. e.
     * adding precompressed file in the zip archive.
     **/
    quint32 crc;
    /** This is only needed if you are using raw file zipping mode, i. e.
     * adding precompressed file in the zip archive.
     **/
    quint64 uncompressedSize;
    /// Comment for this file entry
    QString comment;
    /// Zip file Extra Fields
    QByteArray extraFields;

    /// Default constructor
    QuaZipNewInfo();
    /// Initializes the new instance from existing file info.
    /** \note Mainly used when copying files between archives.*/
    QuaZipNewInfo(const QuaZipFileInfo &existing);

    /// Constructs QuaZipNewInfo instance.
    /** Extracts file name from \a filePath.
     * Initializes other fields that can be provided with QFileInfo(filePath)
     **/
    static QuaZipNewInfo fromFile(const QString &filePath);
    /// Constructs QuaZipNewInfo instance.
    /** File name is set from \storeName
     * Initializes the fields that can be provided with QFileInfo(filePath)
     **/
    static QuaZipNewInfo fromFile(
        const QString &filePath, const QString &storeName);

    /**
     * Initializes the fields that can be provided with QFileInfo(filePath)
     * \returns true when exists and can be accessed
     **/
    bool initWithFile(const QString &filePath);
    /** File name is set from \storeName
     * Initializes the fields that can be provided with QFileInfo(filePath)
     * \returns true when exists and can be accessed
     **/
    bool initWithFile(const QString &filePath, const QString &storeName);

    /// When the file was created
    QDateTime creationTime() const;
    /// Set when the file was created
    /// \param time will be stored in NTFS extra field
    void setCreationTime(const QDateTime &time);

    /// When the file was last accessed
    QDateTime lastAccessTime() const;
    /// Set when the file was last accessed
    /// \param time will be stored in NTFS extra field
    void setLastAccessTime(const QDateTime &time);

    /// Set when the file was modified
    /// \param time will be stored in NTFS extra field
    /// and modificationTime will be overwritten
    void setModificationTime(const QDateTime &time);

    /// Takes the creation time of the existing \a file
    void setCreationTime(const QString &file);
    /// Takes the modification time of the existing \a file
    void setModificationTime(const QString &file);
    /// Takes the last access time of the existing \a file
    void setLastAccessTime(const QString &file);
    /// Takes the access permissions of the existing \a file
    void setPermissions(const QString &file);
};

#endif
