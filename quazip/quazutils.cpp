#include "quazutils.h"

#include <QString>
#include <QFileInfo>
#include <QDir>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

bool QuaZUtils::isAscii(const QString &text)
{
    for (const QChar &ch : text) {
        if (ch.unicode() > 127)
            return false;
    }

    return true;
}

bool QuaZUtils::createSymLink(const QString &linkPath, const QString &target)
{
    bool isDir;
#ifdef Q_OS_WIN
    auto absTarget = QFileInfo(linkPath).dir().absoluteFilePath(target);
    isDir = QFileInfo(absTarget).isDir();
#else
    isDir = false;
#endif
    return createSymLink(linkPath, target, isDir);
}

bool QuaZUtils::createSymLink(
    const QString &linkPath, const QString &target, bool isDir)
{
#ifdef Q_OS_WIN
    DWORD dwFlags = isDir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
#ifdef UNICODE
    static_assert(sizeof(WCHAR) == sizeof(decltype(*target.utf16())),
        "WCHAR size mismatch");

    return CreateSymbolicLinkW(
        reinterpret_cast<const WCHAR *>(linkPath.utf16()),
        reinterpret_cast<const WCHAR *>(target.utf16()), dwFlags);
#else
    return CreateSymbolicLinkA(
        linkPath.toLocal8Bit().data(), absTarget.toLocal8Bit().data(), dwFlags);
#endif
#else
    Q_UNUSED(isDir);
    return QFile::link(target, linkPath);
#endif
}
