#include "scanner.h"

#include <QDirIterator>
#include <QFileInfo>

FileScanner::FileScanner(QObject *parent) : QObject(parent), m_cancelled(0) {}

void FileScanner::cancel()
{
    m_cancelled.storeRelease(1);
}

bool FileScanner::isProtectedPath(const QString &path)
{
    const QString p = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toLower();
    const QString volumeRelative = p.size() >= 3 && p.at(1) == QLatin1Char(':') ? p.mid(3) : p;
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
    const QString name = QFileInfo(p).fileName();
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
