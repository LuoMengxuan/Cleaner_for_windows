#ifndef SCANNER_H
#define SCANNER_H

#include <QObject>
#include <QAtomicInt>

class FileScanner : public QObject
{
    Q_OBJECT
public:
    explicit FileScanner(QObject *parent = nullptr);

public slots:
    void scan(const QString &rootPath, qint64 minimumBytes);
    void cancel();

signals:
    void fileFound(const QString &path, qint64 size, bool protectedFile);
    void progress(qint64 filesVisited, qint64 bytesMatched, const QString &currentPath);
    void finished(qint64 filesVisited, qint64 matchedFiles, qint64 bytesMatched, bool cancelled);

private:
    static bool isProtectedPath(const QString &path);
    QAtomicInt m_cancelled;
};

#endif

