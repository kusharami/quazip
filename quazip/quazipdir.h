#pragma once
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

class QuaZipDirPrivate;

#include "quazip.h"
#include "quazipfileinfo.h"
#include <QDir>
#include <QList>
#include <QSharedDataPointer>

/// Provides ZIP archive navigation.
/**
* This class is modelled after QDir, and is designed to provide similar
* features for ZIP archives.
*
* The only significant difference from QDir is that the root path is not
* '/', but an empty string since that's how the file paths are stored in
* the archive. However, QuaZipDir understands the paths starting with
* '/'. It is important in a few places:
*
* - In the cd() function.
* - In the constructor.
* - In the exists() function.
* - In the relativePath() function.
*
* Note that since ZIP uses '/' on all platforms, the '\' separator is
* not supported.
*/
class QUAZIP_EXPORT QuaZipDir {
private:
    QSharedDataPointer<QuaZipDirPrivate> d;

public:
    /// Default constructor
    QuaZipDir();
    /// The copy constructor.
    QuaZipDir(const QuaZipDir &that);
    /// Constructs a QuaZipDir instance pointing to the specified directory.
    /**
       If \a dir is not specified, points to the root of the archive.
       The same happens if the \a dir is &quot;/&quot;.
     */
    explicit QuaZipDir(QuaZip *zip, const QString &dir = QString());
    /// Destructor.
    ~QuaZipDir();

    QuaZip *zip() const;
    void setZip(QuaZip *zip);

    /// Equality operator.
    bool operator==(const QuaZipDir &that) const;
    /// Inequality operator.
    inline bool operator!=(const QuaZipDir &that) const;
    /// Asignment operator
    QuaZipDir &operator=(const QuaZipDir &that);
    /// Returns the name of the entry at the specified position.
    QString operator[](int pos) const;
    /// Returns the current case sensitivity mode.
    QuaZip::CaseSensitivity caseSensitivity() const;
    /// Changes the 'current' directory.
    /**
      * If the path starts with '/', it is interpreted as an absolute
      * path from the root of the archive. Otherwise, it is interpreted
      * as a path relative to the current directory as was set by the
      * previous cd() or the constructor.
      *
      * Note that the subsequent path() call will not return a path
      * starting with '/' in all cases.
      */
    bool cd(const QString &dirPath);
    /// Goes up.
    bool cdUp();
    /// Returns the number of entries in the directory.
    int count() const;
    /// Returns the current directory name.
    /**
      The name doesn't include the path.
      */
    QString dirName() const;
    /// Returns the list of the entries in the directory with zip64 support.
    /**
      \param nameFilters The list of file patterns to list, uses the same
      syntax as QDir.
      \param filters The entry type filters, only Files and Dirs are
      accepted.
      \param sort Sorting mode.
      */
    QList<QuaZipFileInfo> entryInfoList(const QStringList &nameFilters,
        QDir::Filters filters = QDir::NoFilter,
        QDir::SortFlags sort = QDir::NoSort) const;
    /// Returns the list of the entries in the directory.
    /**
      \overload

      The same as entryInfoList(QStringList(), filters, sort).
      */
    QList<QuaZipFileInfo> entryInfoList(QDir::Filters filters = QDir::NoFilter,
        QDir::SortFlags sort = QDir::NoSort) const;
    /// Returns the list of the entry names in the directory.
    /**
      The same as entryInfoList(nameFilters, filters, sort), but only
      returns entry names.
      */
    QStringList entryList(const QStringList &nameFilters,
        QDir::Filters filters = QDir::NoFilter,
        QDir::SortFlags sort = QDir::NoSort) const;
    /// Returns the list of the entry names in the directory.
    /**
      \overload

      The same as entryList(QStringList(), filters, sort).
      */
    QStringList entryList(QDir::Filters filters = QDir::NoFilter,
        QDir::SortFlags sort = QDir::NoSort) const;
    /// Returns \c true if the entry with the specified name exists.
    /**
      The &quot;..&quot; is considered to exist if the current directory
      is not root. The &quot;.&quot; and &quot;/&quot; are considered to
      always exist. Paths starting with &quot;/&quot; are relative to
      the archive root, other paths are relative to the current dir.
      */
    bool exists(const QString &fileName) const;
    /// Return \c true if the directory pointed by this QuaZipDir exists.
    bool exists() const;
    /// Returns the full path to the specified file.
    /**
      Doesn't check if the file actually exists.
      */
    QString filePath(const QString &fileName) const;
    /// Returns the default filter.
    QDir::Filters filter();
    /// Returns if the QuaZipDir points to the root of the archive.
    /**
      \note The root path is the empty string, not '/'.
     */
    bool isRoot() const;
    /// Return the default name filter.
    QStringList nameFilters() const;
    /// Returns the path to the current dir.
    /**
      The path NEVER starts with '/'
      */
    QString path() const;
    /// Returns the path to the specified file relative to the current dir.
    /**
     * This function is mostly useless, provided only for the sake of
     *  completeness.
     *
     * @param fileName The path to the file, should start with &quot;/&quot;
     *  if relative to the archive root.
     * @return Path relative to the current dir.
     */
    QString relativeFilePath(const QString &fileName) const;

    bool isValid() const;

    /// Sets the default case sensitivity mode.
    void setCaseSensitivity(QuaZip::CaseSensitivity caseSensitivity);
    /// Sets the default filter.
    void setFilter(QDir::Filters value);
    /// Sets the default name filter.
    void setNameFilters(const QStringList &value);
    /// Goes to the specified path.
    /**
      The difference from cd() is that this function never checks if the
      path actually exists and doesn't use relative paths, so it's
      possible to go to the root directory with setPath(&quot;&quot;).

      \note This function chops the trailing '/' and
      treats a single '/' as the root path and removes all redundant '.' and '..'
      if path starts with .. then it is invalid path
      */
    void setPath(const QString &path);
    /// Sets the default sorting mode.
    void setSorting(QDir::SortFlags value);
    /// Returns the default sorting mode.
    QDir::SortFlags sorting() const;
};

bool QuaZipDir::operator!=(const QuaZipDir &that) const
{
    return !operator==(that);
}
