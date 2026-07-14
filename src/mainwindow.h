#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QString>

class QLabel;
class QPushButton;
class QComboBox;
class QTableWidget;
class QVBoxLayout;
class QThread;
class FileScanner;
class QPaintEvent;
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
    void selectAllSafe(bool checked);
    void refreshDrives();
    void showFromTray(QSystemTrayIcon::ActivationReason reason);
    void importBackground();
    void showDeveloperInfo();

signals:
    void requestScan(const QString &rootPath, qint64 minimumBytes);
    void requestCancel();

private:
    static QString formatBytes(qint64 bytes);
    static QIcon makeTrayIcon(int usedPercent);
    bool moveToRecycleBin(const QString &path, QString *errorMessage);
    void setupUi();
    void setupTray();
    void setScanning(bool active);

    Ui::MainWindow *ui;
    QTableWidget *m_table;
    QComboBox *m_driveSelector;
    QComboBox *m_minimumSize;
    QPushButton *m_scanButton;
    QPushButton *m_stopButton;
    QPushButton *m_deleteButton;
    QPushButton *m_openLocationButton;
    QLabel *m_statusLabel;
    QLabel *m_pathLabel;
    QVBoxLayout *m_driveLayout;
    QSystemTrayIcon *m_trayIcon;
    QThread *m_scanThread;
    FileScanner *m_scanner;
    QString m_backgroundPath;
    bool m_scanning;
    bool m_quitting;
};

#endif
