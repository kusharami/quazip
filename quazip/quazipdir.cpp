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

#include "unzip.h"

/// \cond internal
class QuaZipDirPrivate : public QSharedData {
public:
    QuaZipDirPrivate();
    QuaZipDirPrivate(const QuaZipDirPrivate &other);
    QuaZipDirPrivate(QuaZip *zip, const QString &dir = QString());

    QuaZip *zip;
    QString dir;
    QuaZip::CaseSensitivity caseSensitivity;
    QDir::Filters filter;
    QStringList nameFilters;
    QDir::SortFlags sorting;
    template<typename TFileInfoList>
    bool entryList(QStringList nameFilters, QDir::Filters filter,
        QDir::SortFlags sort, TFileInfoList &result) const;

    bool entryInfoList(QStringList nameFilters, QDir::Filters filter,
        QDir::SortFlags sort, QList<QuaZipFileInfo> &result) const;
};
/// \endcond

QuaZipDir::QuaZipDir(const QuaZipDir &that)
    : d(that.d)
{
}

QuaZipDir::QuaZipDir(QuaZip *zip, const QString &dir)
    : d(new QuaZipDirPrivate(zip, dir))
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
        d->dir.clear();
        return true;
    }

    if (normalizedDirPath == ".") {
        return true;
    }

    if (normalizedDirPath == "..") {
        if (isRoot()) {
            return false;
        }
        int slashPos = d->dir.lastIndexOf('/');
        Q_ASSERT(slashPos >= 0);

        d->dir = d->dir.left(slashPos);
        return true;
    }

    if (normalizedDirPath.contains('/')) {
        QuaZipDir dir(*this);
        if (normalizedDirPath.startsWith('/')) {
#ifdef QUAZIP_QUAZIPDIR_DEBUG
            qDebug(
                "QuaZipDir::cd(%s): going to /", dirName.toUtf8().constData());
#endif
            if (!dir.cd(QChar('/')))
                return false;
        }
        QStringList path =
            normalizedDirPath.split('/', QString::SkipEmptyParts);
        for (const auto &step : path) {
#ifdef QUAZIP_QUAZIPDIR_DEBUG
            qDebug("QuaZipDir::cd(%s): going to %s",
                dirName.toUtf8().constData(), step.toUtf8().constData());
#endif
            if (!dir.cd(step))
                return false;
        }
        d->dir = dir.path();
        return true;
    }

    // a simple subdirectory
    if (exists(normalizedDirPath)) {
        normalizedDirPath = '/' + normalizedDirPath;
        if (isRoot())
            d->dir = normalizedDirPath;
        else
            d->dir += normalizedDirPath;
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

static QuaZipFileInfo QuaZipDir_getFileInfo(
    QuaZip *zip, bool *ok, const QString &filePath, bool isReal)
{
    QuaZipFileInfo info;
    if (isReal) {
        *ok = zip->getCurrentFileInfo(info);
    } else {
        *ok = true;
        info.setFilePath(filePath);
        info.setEntryType(QuaZipFileInfo::Directory);
    }
    return info;
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
            return string1.toLower().localeAwareCompare(string2.toLower());
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
        if (info1FilePath.endsWith('/') && !info2FilePath.endsWith('/'))
            return (sort & QDir::DirsFirst) == QDir::DirsFirst;

        if (!info1FilePath.endsWith('/') && info2FilePath.endsWith('/'))
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
    if (!zip)
        return false;

    if (dir.contains(".."))
        return false;

    QString basePath = dir;
    if (!basePath.isEmpty() && !basePath.endsWith('/'))
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
    auto sensitivity = QuaZip::convertCaseSensitivity(caseSensitivity);
    do {
        QString name = zip->currentFilePath();
        if (!name.startsWith(basePath, sensitivity))
            continue;
        QString relativeName = name.mid(baseLength);
        if (relativeName.isEmpty())
            continue;
        bool isDir = false;
        bool isReal = true;
        if (relativeName.contains('/')) {
            int indexOfSlash = relativeName.indexOf('/');
            // something like "subdir/"
            isReal = indexOfSlash == relativeName.length() - 1;
            relativeName = relativeName.left(indexOfSlash + 1);
            isDir = true;
        }
        auto caseName = relativeName;
        if (sensitivity)
            caseName = QLocale().toLower(caseName);
        if (caseName.endsWith('/'))
            caseName.chop(1);
        if (entriesFound.contains(caseName))
            continue;
        entriesFound.insert(caseName);
        if ((fltr & QDir::Dirs) == 0 && isDir)
            continue;
        if ((fltr & QDir::Files) == 0 && !isDir)
            continue;
        if (!nmfltr.isEmpty() && !QDir::match(nmfltr, relativeName))
            continue;
        bool ok;
        QuaZipFileInfo info =
            QuaZipDir_getFileInfo(zip, &ok, basePath + relativeName, isReal);
        if (!ok) {
            return false;
        }
        result.append(info);
    } while (zip->goToNextFile());

    if (zip->zipError() != UNZ_OK)
        return false;

    QDir::SortFlags srt = sort;
    if (srt == QDir::NoSort)
        srt = sorting;

    if (srt != QDir::NoSort && (srt & QDir::Unsorted) != QDir::Unsorted) {
        if (sensitivity == Qt::CaseInsensitive) {
            srt |= QDir::IgnoreCase;
        }
        QuaZipDirComparator lessThan(srt);
        qSort(result.begin(), result.end(), lessThan);
    }
    return true;
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
    if (filePath.isEmpty() || filePath == QChar('/'))
        return true;

    if (normalizedFilePath.endsWith('/'))
        normalizedFilePath.chop(1);

    if (normalizedFilePath == ".") {
        return true;
    }

    if (normalizedFilePath == "..") {
        return !isRoot();
    }

    QFileInfo fileInfo(this->filePath(normalizedFilePath));
    auto fileName = fileInfo.fileName();
    if (normalizedFilePath.contains('/')) {
#ifdef QUAZIP_QUAZIPDIR_DEBUG
        qDebug("QuaZipDir::exists(): fileName=%s, fileInfo.fileName()=%s, "
               "fileInfo.path()=%s",
            fileName.toUtf8().constData(),
            fileInfo.fileName().toUtf8().constData(),
            fileInfo.path().toUtf8().constData());
#endif
        QuaZipDir dir(*this);
        return dir.cd(fileInfo.path()) && dir.exists(fileName);
    }

    auto entries = entryInfoList(QDir::AllEntries, QDir::NoSort);
    Qt::CaseSensitivity cs = QuaZip::convertCaseSensitivity(d->caseSensitivity);
    for (auto &entry : entries) {
        if (entry.fileName().compare(fileName, cs) == 0)
            return true;
    }

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
    return d->dir == QChar('/');
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
    Q_ASSERT(path.startsWith('/'));
    return QDir(path).relativeFilePath(fileName);
}

bool QuaZipDir::isValid() const
{
    return d->zip && !d->dir.contains("..");
}

void QuaZipDir::setCaseSensitivity(QuaZip::CaseSensitivity caseSensitivity)
{
    d->caseSensitivity = caseSensitivity;
}

void QuaZipDir::setFilter(QDir::Filters filters)
{
    d->filter = filters;
}

void QuaZipDir::setNameFilters(const QStringList &nameFilters)
{
    d->nameFilters = nameFilters;
}

void QuaZipDir::setPath(const QString &path)
{
    auto normalizedPath = QDir::cleanPath(path);

    if (!normalizedPath.startsWith('/'))
        normalizedPath = '/' + normalizedPath;

    if (normalizedPath == this->path())
        return;

    d->dir = normalizedPath;
}

void QuaZipDir::setSorting(QDir::SortFlags sort)
{
    d->sorting = sort;
}

QDir::SortFlags QuaZipDir::sorting() const
{
    return d->sorting;
}

QuaZipDirPrivate::QuaZipDirPrivate()
    : QuaZipDirPrivate(nullptr, QString())
{
}

QuaZipDirPrivate::QuaZipDirPrivate(const QuaZipDirPrivate &other)
    : zip(other.zip)
    , dir(other.dir)
    , caseSensitivity(other.caseSensitivity)
    , filter(other.filter)
    , nameFilters(other.nameFilters)
    , sorting(other.sorting)
{
}

QuaZipDirPrivate::QuaZipDirPrivate(QuaZip *zip, const QString &dir)
    : zip(zip)
    , dir(dir.isEmpty() ? QString('/') : dir)
    , caseSensitivity(QuaZip::csDefault)
    , filter(QDir::NoFilter)
    , sorting(QDir::NoSort)
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
