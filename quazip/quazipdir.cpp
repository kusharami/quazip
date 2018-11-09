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

#include "quazipdir.h"

#include <QSet>
#include <QSharedData>
#include <QLocale>
#include <QDateTime>

#include "unzip.h"

/// \cond internal
class QuaZipDirPrivate : public QSharedData {
public:
    QuaZipDirPrivate();
    QuaZipDirPrivate(const QuaZipDirPrivate &other);
    QuaZipDirPrivate(QuaZip *zip);

    QuaZip *zip;
    QString dir;
    QDir::Filters filter;
    QStringList nameFilters;
    QDir::SortFlags sorting;
    QuaZip::CaseSensitivity caseSensitivity;
    template<typename TFileInfoList>
    bool entryList(QStringList nameFilters, QDir::Filters filter,
        QDir::SortFlags sort, TFileInfoList &result) const;

    bool entryInfoList(QStringList nameFilters, QDir::Filters filter,
        QDir::SortFlags sort, QList<QuaZipFileInfo> &result) const;

    bool isValid() const;
};
/// \endcond

QuaZipDir::QuaZipDir(const QuaZipDir &that)
    : d(that.d)
{
}

QuaZipDir::QuaZipDir(QuaZip *zip, const QString &dir)
    : d(new QuaZipDirPrivate(zip))
{
    setPath(dir);
}

QuaZipDir::QuaZipDir()
    : d(new QuaZipDirPrivate)
{
}

QuaZipDir::~QuaZipDir()
{
    // destruct shared data
}

QuaZip *QuaZipDir::zip() const
{
    return d->zip;
}

void QuaZipDir::setZip(QuaZip *zip)
{
    if (d.constData()->zip == zip)
        return;

    d->zip = zip;
}

bool QuaZipDir::operator==(const QuaZipDir &that) const
{
    return d == that.d ||
        (d->zip == that.d->zip &&
            d->caseSensitivity == that.d->caseSensitivity &&
            d->dir == that.d->dir && d->filter == that.d->filter &&
            d->nameFilters == that.d->nameFilters &&
            d->sorting == that.d->sorting);
}

QuaZipDir &QuaZipDir::operator=(const QuaZipDir &that)
{
    this->d = that.d;
    return *this;
}

QString QuaZipDir::operator[](int pos) const
{
    return entryList().at(pos);
}

QuaZip::CaseSensitivity QuaZipDir::caseSensitivity() const
{
    return d->caseSensitivity;
}

bool QuaZipDir::cd(const QString &dirPath)
{
    QString normalizedDirPath = QDir::cleanPath(dirPath);
    if (normalizedDirPath.isEmpty() || normalizedDirPath == QChar('/')) {
        if (isValid()) {
            d->dir.clear();
            return true;
        }
        return false;
    }

    if (normalizedDirPath == ".") {
        return exists();
    }

    if (normalizedDirPath == "..") {
        if (isRoot()) {
            return false;
        }
        int slashPos = d->dir.lastIndexOf('/');
        Q_ASSERT(slashPos != 0);
        QString newPath;
        if (slashPos > 0)
            newPath = d->dir.left(slashPos);
        if (exists('/' + newPath)) {
            d->dir = newPath;
            return true;
        }
        return false;
    }

    if (normalizedDirPath.contains('/')) {
        QuaZipDir dir(*this);
        if (normalizedDirPath.startsWith('/')) {
            if (!dir.cd(QChar('/')))
                return false;
        }
        QStringList path =
            normalizedDirPath.split('/', QString::SkipEmptyParts);
        for (const auto &step : path) {
            if (!dir.cd(step))
                return false;
        }
        d->dir = dir.path();
        return true;
    }

    // a simple subdirectory
    if (exists(normalizedDirPath)) {
        if (isRoot())
            d->dir = normalizedDirPath;
        else
            d->dir += '/' + normalizedDirPath;
        return true;
    }

    return false;
}

bool QuaZipDir::cdUp()
{
    return cd("..");
}

int QuaZipDir::count() const
{
    return entryInfoList().count();
}

QString QuaZipDir::dirName() const
{
    return QDir(d->dir).dirName();
}

static void QuaZipDir_convertInfoList(
    const QList<QuaZipFileInfo> &from, QList<QuaZipFileInfo> &to)
{
    to = from;
}

static void QuaZipDir_convertInfoList(
    const QList<QuaZipFileInfo> &from, QStringList &to)
{
    to.clear();
    for (auto &info : from) {
        to.append(info.fileName());
    }
}

/// \cond internal
/**
  An utility class to restore the current file.
  */
class QuaZipDirRestoreCurrent {
public:
    inline QuaZipDirRestoreCurrent(QuaZip *zip);
    inline ~QuaZipDirRestoreCurrent();

private:
    QuaZip *zip;
    QString currentFile;
};
/// \endcond

/// \cond internal
class QuaZipDirComparator {
private:
    QDir::SortFlags sort;
    static QString getExtension(const QString &name);
    int compareStrings(const QString &string1, const QString &string2);

public:
    inline QuaZipDirComparator(QDir::SortFlags sort);
    bool operator()(const QuaZipFileInfo &info1, const QuaZipFileInfo &info2);
};

QString QuaZipDirComparator::getExtension(const QString &name)
{
    if (name.endsWith('.') || name.indexOf('.', 1) == -1) {
        return QString();
    }

    return name.mid(name.lastIndexOf('.') + 1);
}

int QuaZipDirComparator::compareStrings(
    const QString &string1, const QString &string2)
{
    if (sort & QDir::LocaleAware) {
        if (sort & QDir::IgnoreCase) {
            return QLocale().toLower(string1).localeAwareCompare(
                QLocale().toLower(string2));
        }
        return string1.localeAwareCompare(string2);
    }

    return string1.compare(string2,
        (sort & QDir::IgnoreCase) ? Qt::CaseInsensitive : Qt::CaseSensitive);
}

QuaZipDirComparator::QuaZipDirComparator(QDir::SortFlags sort)
    : sort(sort)
{
}

bool QuaZipDirComparator::operator()(
    const QuaZipFileInfo &info1, const QuaZipFileInfo &info2)
{
    auto &info1FilePath = info1.filePath();
    auto &info2FilePath = info2.filePath();
    QDir::SortFlags order =
        sort & (QDir::Name | QDir::Time | QDir::Size | QDir::Type);
    if ((sort & QDir::DirsFirst) == QDir::DirsFirst ||
        (sort & QDir::DirsLast) == QDir::DirsLast) {
        if (info1.isDir() && !info2.isDir())
            return (sort & QDir::DirsFirst) == QDir::DirsFirst;

        if (!info1.isDir() && info2.isDir())
            return (sort & QDir::DirsLast) == QDir::DirsLast;
    }
    bool result;
    int extDiff;
    switch (order) {
    case QDir::Name:
        result = compareStrings(info1FilePath, info2FilePath) < 0;
        break;
    case QDir::Type:
        extDiff = compareStrings(
            getExtension(info1FilePath), getExtension(info2FilePath));
        if (extDiff == 0) {
            result = compareStrings(info1FilePath, info2FilePath) < 0;
        } else {
            result = extDiff < 0;
        }
        break;
    case QDir::Size:
        if (info1.uncompressedSize() == info2.uncompressedSize()) {
            result = compareStrings(info1FilePath, info2FilePath) < 0;
        } else {
            result = info1.uncompressedSize() < info2.uncompressedSize();
        }
        break;
    case QDir::Time:
        if (info1.modificationTime() == info2.modificationTime()) {
            result = compareStrings(info1FilePath, info2FilePath) < 0;
        } else {
            result = info1.modificationTime() < info2.modificationTime();
        }
        break;
    default:
        qWarning(
            "QuaZipDirComparator(): Invalid sort mode 0x%2X", unsigned(sort));
        return false;
    }
    return (sort & QDir::Reversed) ? !result : result;
}

template<typename TFileInfoList>
bool QuaZipDirPrivate::entryList(QStringList nameFilters, QDir::Filters filter,
    QDir::SortFlags sort, TFileInfoList &result) const
{
    result.clear();
    QList<QuaZipFileInfo> list;
    if (entryInfoList(nameFilters, filter, sort, list)) {
        QuaZipDir_convertInfoList(list, result);
        return true;
    }

    return false;
}

bool QuaZipDirPrivate::entryInfoList(QStringList nameFilters,
    QDir::Filters filter, QDir::SortFlags sort,
    QList<QuaZipFileInfo> &result) const
{
    result.clear();
    if (!isValid())
        return false;

    QString basePath = dir;
    if (!basePath.isEmpty())
        basePath += '/';
    int baseLength = basePath.length();
    QuaZipDirRestoreCurrent saveCurrent(zip);
    if (!zip->goToFirstFile()) {
        return zip->zipError() == UNZ_OK;
    }
    QDir::Filters fltr = filter;
    if (fltr == QDir::NoFilter)
        fltr = this->filter;
    if (fltr == QDir::NoFilter)
        fltr = QDir::AllEntries;
    QStringList nmfltr = nameFilters;
    if (nmfltr.isEmpty())
        nmfltr = this->nameFilters;
    QSet<QString> entriesFound;
    QString basePathLower;
    bool caseInsensitive = !(fltr & QDir::CaseSensitive) &&
        QuaZip::convertCaseSensitivity(caseSensitivity) == Qt::CaseInsensitive;
    if (caseInsensitive)
        basePathLower = QLocale().toLower(basePath);
    do {
        auto name = zip->currentFilePath();
        if (caseInsensitive) {
            if (!QLocale().toLower(name).startsWith(basePathLower))
                continue;
        } else {
            if (!name.startsWith(basePath))
                continue;
        }
        QString relativeName = name.mid(baseLength);
        if (relativeName.isEmpty())
            continue;
        bool isDir = false;
        bool isReal = true;
        if (relativeName.contains('/')) {
            int indexOfSlash = relativeName.indexOf('/');
            // something like "subdir/"
            isReal = indexOfSlash == relativeName.length() - 1;
            relativeName = relativeName.left(indexOfSlash); //name without slash
            isDir = true;
        }
        auto caseName = relativeName;
        if (caseInsensitive)
            caseName = QLocale().toLower(caseName);
        if (entriesFound.contains(caseName))
            continue;
        entriesFound.insert(caseName);

        if (!isDir && !(fltr & QDir::Files))
            continue;
        if (isDir && !(fltr & (QDir::Dirs | QDir::AllDirs)))
            continue;
        if (!nmfltr.isEmpty() && !QDir::match(nmfltr, relativeName))
            continue;
        QuaZipFileInfo info;
        if (isReal) {
            if (!zip->getCurrentFileInfo(info))
                return false;
            if (!(fltr & QDir::Hidden) && info.isHidden())
                continue;
            if (!(fltr & QDir::System) && info.isSystem())
                continue;
            if ((fltr & QDir::NoSymLinks) && info.isSymLink())
                continue;
            if ((fltr & QDir::Readable) && !info.isReadable())
                continue;
            if ((fltr & QDir::Writable) && !info.isWritable())
                continue;
            if ((fltr & QDir::Executable) && !info.isExecutable())
                continue;
            if ((fltr & QDir::Modified) &&
                info.creationTime() < info.modificationTime())
                continue;
        } else {
            info.setFilePath(basePath + relativeName);
            info.setEntryType(QuaZipFileInfo::Directory);
        }

        result.append(info);
    } while (zip->goToNextFile());

    if (zip->zipError() != UNZ_OK)
        return false;

    QDir::SortFlags srt = sort;
    if (srt == QDir::NoSort)
        srt = sorting;

    if (srt != QDir::NoSort && (srt & QDir::Unsorted) != QDir::Unsorted) {
        if (caseInsensitive) {
            srt |= QDir::IgnoreCase;
        }
        QuaZipDirComparator lessThan(srt);
        qSort(result.begin(), result.end(), lessThan);
    }
    return true;
}

bool QuaZipDirPrivate::isValid() const
{
    if (!zip)
        return false;

    if (zip->openMode() != QuaZip::mdUnzip)
        return false;

    if (zip->zipError() != UNZ_OK)
        return false;

    return !dir.startsWith('/') && !dir.endsWith('/') && dir != ".." &&
        !dir.contains("../");
}

/// \endcond

QList<QuaZipFileInfo> QuaZipDir::entryInfoList(const QStringList &nameFilters,
    QDir::Filters filters, QDir::SortFlags sort) const
{
    QList<QuaZipFileInfo> result;
    if (d->entryList(nameFilters, filters, sort, result))
        return result;

    return QList<QuaZipFileInfo>();
}

QList<QuaZipFileInfo> QuaZipDir::entryInfoList(
    QDir::Filters filters, QDir::SortFlags sort) const
{
    return entryInfoList(QStringList(), filters, sort);
}

QStringList QuaZipDir::entryList(const QStringList &nameFilters,
    QDir::Filters filters, QDir::SortFlags sort) const
{
    QStringList result;
    if (d->entryList(nameFilters, filters, sort, result))
        return result;

    return QStringList();
}

QStringList QuaZipDir::entryList(
    QDir::Filters filters, QDir::SortFlags sort) const
{
    return entryList(QStringList(), filters, sort);
}

bool QuaZipDir::exists(const QString &filePath) const
{
    QString normalizedFilePath = QDir::cleanPath(filePath);
    if (filePath.isEmpty() || normalizedFilePath == QChar('/'))
        return isValid();

    if (normalizedFilePath == "..") {
        if (isRoot())
            return false;
    }

    auto zip = d->zip;
    QuaZipDirRestoreCurrent saveCurrent(zip);
    if (!zip->goToFirstFile()) {
        return false;
    }

    bool isDir = filePath.endsWith('/') || filePath.endsWith('\\') ||
        normalizedFilePath == "..";
    normalizedFilePath = QDir::cleanPath(this->filePath(normalizedFilePath));
    if (normalizedFilePath.startsWith('/'))
        normalizedFilePath = normalizedFilePath.mid(1);
    if (normalizedFilePath.startsWith('/') ||
        normalizedFilePath.startsWith("../") || normalizedFilePath == "..")
        return false;

    bool caseInsensitive = Qt::CaseInsensitive ==
        QuaZip::convertCaseSensitivity(d->caseSensitivity);
    if (caseInsensitive)
        normalizedFilePath = QLocale().toLower(normalizedFilePath);
    int pathLength = normalizedFilePath.length();
    do {
        auto name = zip->currentFilePath();
        if (caseInsensitive)
            name = QLocale().toLower(name);
        if (name.startsWith(normalizedFilePath)) {
            if (name.length() == pathLength) {
                return !isDir;
            }

            if (name.at(pathLength) == '/')
                return true;
        }

    } while (zip->goToNextFile());

    return false;
}

bool QuaZipDir::exists() const
{
    return QuaZipDir(d->zip).exists(d->dir);
}

QString QuaZipDir::filePath(const QString &fileName) const
{
    if (QDir::cleanPath(fileName).startsWith('/'))
        return fileName;

    return QDir(d->dir).filePath(fileName);
}

QDir::Filters QuaZipDir::filter()
{
    return d->filter;
}

bool QuaZipDir::isRoot() const
{
    return d->dir.isEmpty();
}

QStringList QuaZipDir::nameFilters() const
{
    return d->nameFilters;
}

QString QuaZipDir::path() const
{
    return d->dir;
}

QString QuaZipDir::relativeFilePath(const QString &fileName) const
{
    auto path = d->dir;
    Q_ASSERT(!path.startsWith('/'));
    path = '/' + path;
    return QDir(path).relativeFilePath(fileName);
}

bool QuaZipDir::isValid() const
{
    return d->isValid();
}

void QuaZipDir::setFilter(QDir::Filters value)
{
    if (filter() == value)
        return;

    d->filter = value;
}

void QuaZipDir::setNameFilters(const QStringList &value)
{
    if (nameFilters() == value)
        return;

    d->nameFilters = value;
}

void QuaZipDir::setPath(const QString &path)
{
    auto normalizedPath = QDir::cleanPath(path);

    if (normalizedPath.startsWith('/'))
        normalizedPath = normalizedPath.mid(1);

    if (normalizedPath == this->path())
        return;

    d->dir = normalizedPath;
}

void QuaZipDir::setSorting(QDir::SortFlags value)
{
    if (sorting() == value)
        return;

    d->sorting = value;
}

QDir::SortFlags QuaZipDir::sorting() const
{
    return d->sorting;
}

void QuaZipDir::setCaseSensitivity(QuaZip::CaseSensitivity value)
{
    if (caseSensitivity() == value)
        return;

    d->caseSensitivity = value;
}

QuaZipDirPrivate::QuaZipDirPrivate()
    : QuaZipDirPrivate(nullptr)
{
}

QuaZipDirPrivate::QuaZipDirPrivate(const QuaZipDirPrivate &other)
    : zip(other.zip)
    , dir(other.dir)
    , filter(other.filter)
    , nameFilters(other.nameFilters)
    , sorting(other.sorting)
    , caseSensitivity(other.caseSensitivity)
{
}

QuaZipDirPrivate::QuaZipDirPrivate(QuaZip *zip)
    : zip(zip)
    , filter(QDir::NoFilter)
    , sorting(QDir::NoSort)
    , caseSensitivity(QuaZip::csDefault)
{
}

QuaZipDirRestoreCurrent::QuaZipDirRestoreCurrent(QuaZip *zip)
    : zip(zip)
    , currentFile(zip->currentFilePath())
{
}

QuaZipDirRestoreCurrent::~QuaZipDirRestoreCurrent()
{
    zip->setCurrentFile(currentFile);
}
