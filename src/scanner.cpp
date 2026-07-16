#include "scanner.h"

#include <QDirIterator>
#include <QFileInfo>

namespace {
bool isDisposableCachePath(const QString &volumeRelativePath)
{
    const bool isUserTemp = volumeRelativePath.startsWith(QStringLiteral("users\\")) &&
        (volumeRelativePath.contains(QStringLiteral("\\appdata\\local\\temp\\")) ||
         volumeRelativePath.contains(QStringLiteral("\\cache\\")) ||
         volumeRelativePath.contains(QStringLiteral("\\caches\\")) ||
         volumeRelativePath.contains(QStringLiteral("\\cache2\\")) ||
         volumeRelativePath.contains(QStringLiteral("\\crashdumps\\")));
    const QString suffix = QFileInfo(volumeRelativePath).suffix().toLower();
    return isUserTemp || (volumeRelativePath.startsWith(QStringLiteral("users\\")) &&
                          (suffix == QStringLiteral("tmp") || suffix == QStringLiteral("temp") ||
                           suffix == QStringLiteral("dmp")));
}
}

FileScanner::FileScanner(QObject *parent) : QObject(parent), m_cancelled(0) {}

void FileScanner::cancel()
{
    m_cancelled.storeRelease(1);
}

bool FileScanner::isProtectedPath(const QString &path)
{
    const QString p = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toLower();
    const QString volumeRelative = p.size() >= 3 && p.at(1) == QLatin1Char(':') ? p.mid(3) : p;
    if (isDisposableCachePath(volumeRelative))
        return false;
    const QStringList protectedRoots = {
        QStringLiteral("windows\\"),
        QStringLiteral("program files\\"),
        QStringLiteral("program files (x86)\\"),
        QStringLiteral("programdata\\"),
        QStringLiteral("system volume information\\"),
        QStringLiteral("$recycle.bin\\")
    };
    for (const QString &root : protectedRoots) {
        if (volumeRelative.startsWith(root))
            return true;
    }
    if (volumeRelative.startsWith(QStringLiteral("users\\")) &&
        (volumeRelative.contains(QStringLiteral("\\appdata\\")) ||
         volumeRelative.contains(QStringLiteral("\\ntuser."))))
        return true;

    const QFileInfo info(p);
    const QString name = info.fileName();
    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("exe") || suffix == QStringLiteral("dll") ||
        suffix == QStringLiteral("sys") || suffix == QStringLiteral("drv") ||
        suffix == QStringLiteral("msi") || suffix == QStringLiteral("bat") ||
        suffix == QStringLiteral("cmd") || suffix == QStringLiteral("ps1"))
        return true;

    if (QStringList({QStringLiteral("ini"), QStringLiteral("cfg"), QStringLiteral("conf"),
                     QStringLiteral("config"), QStringLiteral("json"), QStringLiteral("xml"),
                     QStringLiteral("yml"), QStringLiteral("yaml"), QStringLiteral("db"),
                     QStringLiteral("sqlite"), QStringLiteral("dat"), QStringLiteral("lic"),
                     QStringLiteral("key"), QStringLiteral("pem"), QStringLiteral("pfx")}).contains(suffix))
        return true;

    if (volumeRelative.contains(QStringLiteral("\\config\\")) ||
        volumeRelative.contains(QStringLiteral("\\settings\\")) ||
        volumeRelative.contains(QStringLiteral("\\profiles\\")) ||
        volumeRelative.contains(QStringLiteral("\\userdata\\")))
        return true;

    return name == QStringLiteral("pagefile.sys") ||
           name == QStringLiteral("hiberfil.sys") ||
           name == QStringLiteral("swapfile.sys") ||
           name == QStringLiteral("bootmgr");
}

void FileScanner::scan(const QString &rootPath, qint64 minimumBytes)
{
    m_cancelled.storeRelease(0);
    qint64 visited = 0;
    qint64 matched = 0;
    qint64 matchedBytes = 0;

    QDirIterator it(rootPath,
                    QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (m_cancelled.loadAcquire())
            break;
        const QString path = it.next();
        const QFileInfo info = it.fileInfo();
        ++visited;
        if (!info.isSymLink() && info.size() >= minimumBytes) {
            ++matched;
            matchedBytes += info.size();
            emit fileFound(info.absoluteFilePath(), info.size(), isProtectedPath(path));
        }
        if ((visited % 300) == 0)
            emit progress(visited, matchedBytes, path);
    }
    emit finished(visited, matched, matchedBytes, m_cancelled.loadAcquire() != 0);
}
