#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>

class QLabel;
class QPushButton;
class QComboBox;
class QTableWidget;
class QVBoxLayout;
class QProgressBar;
class QThread;
class FileScanner;
class QPaintEvent;
class QCloseEvent;
class QPoint;

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void startScan();
    void stopScan();
    void addFile(const QString &path, qint64 size, bool protectedFile);
    void updateProgress(qint64 visited, qint64 bytes, const QString &path);
    void scanFinished(qint64 visited, qint64 matched, qint64 bytes, bool cancelled);
    void deleteSelected();
    void openSelectedFileLocation();
    void copySelectedPath();
    void selectAllSafe(bool checked);
    void filterSafeFiles(bool enabled);
    void showFileContextMenu(const QPoint &pos);
    void showCategoryReview();
    void clearResults();
    void refreshDrives();
    void updateSelectionState();
    void showFromTray(QSystemTrayIcon::ActivationReason reason);
    void showBeginnerGuide();
    void showDeveloperInfo();

signals:
    void requestScan(const QString &rootPath, qint64 minimumBytes);
    void requestCancel();

private:
    static QString formatBytes(qint64 bytes);
    static QString fileCategory(const QString &path);
    static QIcon makeTrayIcon(int usedPercent);
    bool moveToRecycleBin(const QString &path, QString *errorMessage);
    void setupUi();
    void setupTray();
    void setScanning(bool active);
    void resetSelectionUi();
    void showFriendlyMessage(const QString &title, const QString &message,
                             const QString &details = QString());

    Ui::MainWindow *ui;
    QTableWidget *m_table;
    QComboBox *m_driveSelector;
    QComboBox *m_minimumSize;
    QPushButton *m_scanButton;
    QPushButton *m_stopButton;
    QPushButton *m_deleteButton;
    QPushButton *m_openLocationButton;
    QPushButton *m_copyPathButton;
    QPushButton *m_categoryReviewButton;
    QLabel *m_statusLabel;
    QLabel *m_pathLabel;
    QLabel *m_resultSummaryLabel;
    QLabel *m_scanProgressLabel;
    QProgressBar *m_scanProgress;
    QVBoxLayout *m_driveLayout;
    QSystemTrayIcon *m_trayIcon;
    QThread *m_scanThread;
    FileScanner *m_scanner;
    bool m_scanning;
    bool m_quitting;
};

#endif
