#include "mainwindow.h"
#include "scanner.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QStorageInfo>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QFileDialog>
#include <QSettings>
#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_table(nullptr), m_driveSelector(nullptr), m_trayIcon(nullptr),
      m_scanThread(new QThread(this)), m_scanner(new FileScanner),
      m_scanning(false), m_quitting(false)
{
    setupUi();
    setupTray();

    m_scanner->moveToThread(m_scanThread);
    connect(m_scanThread, &QThread::finished, m_scanner, &QObject::deleteLater);
    connect(this, &MainWindow::requestScan, m_scanner, &FileScanner::scan);
    connect(this, &MainWindow::requestCancel, m_scanner, &FileScanner::cancel, Qt::DirectConnection);
    connect(m_scanner, &FileScanner::fileFound, this, &MainWindow::addFile);
    connect(m_scanner, &FileScanner::progress, this, &MainWindow::updateProgress);
    connect(m_scanner, &FileScanner::finished, this, &MainWindow::scanFinished);
    m_scanThread->start();

    refreshDrives();
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::refreshDrives);
    timer->start(15000);
}

MainWindow::~MainWindow()
{
    emit requestCancel();
    m_scanThread->quit();
    m_scanThread->wait(3000);
    delete ui;
}

void MainWindow::setupUi()
{
    ui->setupUi(this);
    setWindowIcon(QIcon(QStringLiteral(":/images/app_icon.png")));
    ui->centralWidget->setAttribute(Qt::WA_TranslucentBackground);
    ui->titleLabel->setObjectName(QStringLiteral("title"));
    ui->subtitleLabel->setObjectName(QStringLiteral("subtitle"));
    ui->driveCard->setObjectName(QStringLiteral("card"));
    ui->pathLabel->setObjectName(QStringLiteral("subtitle"));
    ui->scanButton->setObjectName(QStringLiteral("primary"));

    m_table = ui->fileTableWidget;
    m_driveSelector = ui->driveComboBox;
    m_minimumSize = ui->minimumSizeComboBox;
    m_scanButton = ui->scanButton;
    m_stopButton = ui->stopButton;
    m_deleteButton = ui->deleteButton;
    m_openLocationButton = ui->openLocationButton;
    m_statusLabel = ui->statusLabel;
    m_pathLabel = ui->pathLabel;
    m_driveLayout = ui->driveLayout;
    m_backgroundPath = QSettings().value(QStringLiteral("appearance/backgroundPath")).toString();
    if (!m_backgroundPath.isEmpty() && !QFileInfo::exists(m_backgroundPath))
        m_backgroundPath.clear();

    m_minimumSize->addItem(QStringLiteral("100 MB"), 100LL * 1024 * 1024);
    m_minimumSize->addItem(QStringLiteral("500 MB"), 500LL * 1024 * 1024);
    m_minimumSize->addItem(QStringLiteral("1 GB"), 1024LL * 1024 * 1024);
    m_minimumSize->addItem(QStringLiteral("2 GB"), 2LL * 1024 * 1024 * 1024);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setSortingEnabled(true);
    setStyleSheet(QStringLiteral(
        "QMainWindow { background:transparent; }"
        "QWidget#centralWidget { background:transparent; }"
        "QWidget { color:#1f2937; }"
        "#title { font-size:24px; font-weight:700; color:#123a66; }"
        "#subtitle { color:#64748b; }"
        "#card { background:rgba(255,255,255,210); border:1px solid rgba(219,229,239,220); border-radius:9px; }"
        "QTableWidget { background:rgba(255,255,255,225); border:1px solid #dbe5ef; border-radius:7px; gridline-color:#edf2f7; }"
        "QHeaderView::section { background:rgba(234,242,251,240); padding:7px; border:0; border-right:1px solid #dbe5ef; font-weight:600; }"
        "QPushButton { padding:7px 13px; border:1px solid #b9c8d8; border-radius:6px; background:rgba(255,255,255,235); }"
        "QPushButton:hover { background:#eef6ff; }"
        "QPushButton:disabled { color:#9aa6b2; background:#eef1f4; }"
        "QPushButton#primary { color:white; background:#1677d2; border-color:#1677d2; font-weight:600; }"
        "QProgressBar { border:1px solid #cbd5e1; border-radius:5px; background:#eef2f6; text-align:center; }"
        "QProgressBar::chunk { background:#2997e8; border-radius:4px; }"));

    connect(m_scanButton, &QPushButton::clicked, this, &MainWindow::startScan);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopScan);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    connect(m_openLocationButton, &QPushButton::clicked, this, &MainWindow::openSelectedFileLocation);
    connect(ui->selectAllCheckBox, &QCheckBox::toggled, this, &MainWindow::selectAllSafe);
    connect(m_driveSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString root = m_driveSelector->currentData().toString();
        m_scanButton->setText(root.isEmpty() ? QStringLiteral("扫描所选盘") : QStringLiteral("扫描 %1").arg(QDir::toNativeSeparators(root)));
    });

    QAction *developerAction = menuBar()->addAction(QStringLiteral("开发者信息"));
    QAction *importAction = menuBar()->addAction(QStringLiteral("导入背景"));
    connect(developerAction, &QAction::triggered, this, &MainWindow::showDeveloperInfo);
    connect(importAction, &QAction::triggered, this, &MainWindow::importBackground);
}

void MainWindow::setupTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    QMenu *menu = new QMenu(this);
    QAction *showAction = menu->addAction(QStringLiteral("打开VIncinzo 磁盘清理"));
    QAction *scanAction = menu->addAction(QStringLiteral("扫描所选盘"));
    menu->addSeparator();
    QAction *quitAction = menu->addAction(QStringLiteral("退出"));
    connect(showAction, &QAction::triggered, this, [this] { showNormal(); raise(); activateWindow(); });
    connect(scanAction, &QAction::triggered, this, &MainWindow::startScan);
    connect(quitAction, &QAction::triggered, this, [this] { m_quitting = true; qApp->quit(); });
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::showFromTray);
    m_trayIcon->setContextMenu(menu);
    m_trayIcon->setIcon(makeTrayIcon(0));
    m_trayIcon->show();
}

QString MainWindow::formatBytes(qint64 bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = double(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) { value /= 1024.0; ++unit; }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 2).arg(QString::fromLatin1(units[unit]));
}

QIcon MainWindow::makeTrayIcon(int usedPercent)
{
    QPixmap pixmap = QIcon(QStringLiteral(":/images/app_icon.png")).pixmap(64, 64);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QColor color = usedPercent >= 90 ? QColor("#e5484d") : usedPercent >= 75 ? QColor("#f59e0b") : QColor("#1677d2");
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(color);
    p.drawEllipse(34, 35, 28, 28);
    p.setPen(Qt::white);
    QFont f(QStringLiteral("Arial"), 7, QFont::Bold);
    p.setFont(f);
    p.drawText(QRect(34, 35, 28, 28), Qt::AlignCenter, QString::number(usedPercent));
    return QIcon(pixmap);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPixmap source(m_backgroundPath);
    if (source.isNull())
        source.load(QStringLiteral(":/images/background_cartoon.png"));
    if (!source.isNull()) {
        const QPixmap cover = source.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int x = (cover.width() - width()) / 2;
        const int y = (cover.height() - height()) / 2;
        painter.drawPixmap(rect(), cover, QRect(x, y, width(), height()));
    }
    painter.fillRect(rect(), QColor(235, 245, 255, 72));
}

void MainWindow::refreshDrives()
{
    while (QLayoutItem *item = m_driveLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    QStringList tips;
    int cUsed = 0;
    const QString previousRoot = m_driveSelector->currentData().toString();
    m_driveSelector->blockSignals(true);
    m_driveSelector->clear();
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &drive : volumes) {
        if (!drive.isValid() || !drive.isReady() || drive.bytesTotal() <= 0)
            continue;
        const QString root = QDir::toNativeSeparators(drive.rootPath());
        const qint64 used = drive.bytesTotal() - drive.bytesAvailable();
        const int percent = int((used * 100) / drive.bytesTotal());
        const QString label = drive.displayName().isEmpty() ? root : QStringLiteral("%1  %2").arg(root, drive.displayName());
        m_driveSelector->addItem(label, drive.rootPath());
        QWidget *row = new QWidget;
        QHBoxLayout *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 1, 0, 1);
        QLabel *name = new QLabel(QStringLiteral("%1  %2 可用 / %3").arg(root, formatBytes(drive.bytesAvailable()), formatBytes(drive.bytesTotal())));
        name->setMinimumWidth(260);
        QProgressBar *bar = new QProgressBar;
        bar->setRange(0, 100);
        bar->setValue(percent);
        bar->setFormat(QStringLiteral("已用 %1%").arg(percent));
        layout->addWidget(name);
        layout->addWidget(bar, 1);
        m_driveLayout->addWidget(row);
        tips << QStringLiteral("%1 可用 %2 / %3").arg(root, formatBytes(drive.bytesAvailable()), formatBytes(drive.bytesTotal()));
        if (root.startsWith(QStringLiteral("C:"), Qt::CaseInsensitive))
            cUsed = percent;
    }
    int selectedIndex = m_driveSelector->findData(previousRoot);
    if (selectedIndex < 0) {
        for (int i = 0; i < m_driveSelector->count(); ++i) {
            if (m_driveSelector->itemData(i).toString().startsWith(QStringLiteral("C:"), Qt::CaseInsensitive)) {
                selectedIndex = i;
                break;
            }
        }
    }
    if (selectedIndex < 0 && m_driveSelector->count() > 0)
        selectedIndex = 0;
    m_driveSelector->setCurrentIndex(selectedIndex);
    m_driveSelector->blockSignals(false);
    const QString selectedRoot = m_driveSelector->currentData().toString();
    m_scanButton->setText(selectedRoot.isEmpty() ? QStringLiteral("扫描所选盘") : QStringLiteral("扫描 %1").arg(QDir::toNativeSeparators(selectedRoot)));
    m_trayIcon->setIcon(makeTrayIcon(cUsed));
    m_trayIcon->setToolTip(QStringLiteral("VIncinzo 磁盘清理\n") + tips.join(QStringLiteral("\n")));
}

void MainWindow::startScan()
{
    if (m_scanning)
        return;
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    m_table->setSortingEnabled(true);
    setScanning(true);
    const QString rootPath = m_driveSelector->currentData().toString();
    if (rootPath.isEmpty()) {
        setScanning(false);
        QMessageBox::warning(this, QStringLiteral("没有可用磁盘"), QStringLiteral("请选择一个可访问的磁盘。"));
        return;
    }
    m_statusLabel->setText(QStringLiteral("正在扫描 %1，请稍候……").arg(QDir::toNativeSeparators(rootPath)));
    emit requestScan(rootPath, m_minimumSize->currentData().toLongLong());
}

void MainWindow::stopScan()
{
    if (m_scanning) {
        emit requestCancel();
        m_statusLabel->setText(QStringLiteral("正在停止扫描……"));
    }
}

void MainWindow::setScanning(bool active)
{
    m_scanning = active;
    m_scanButton->setEnabled(!active);
    m_stopButton->setEnabled(active);
    m_minimumSize->setEnabled(!active);
    m_driveSelector->setEnabled(!active);
    m_deleteButton->setEnabled(!active);
    m_openLocationButton->setEnabled(!active);
}

void MainWindow::importBackground()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入背景图片"),
        m_backgroundPath.isEmpty() ? QDir::homePath() : QFileInfo(m_backgroundPath).absolutePath(),
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.bmp *.webp);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;
    if (QPixmap(path).isNull()) {
        QMessageBox::warning(this, QStringLiteral("无法导入"), QStringLiteral("所选文件不是可读取的图片。"));
        return;
    }
    m_backgroundPath = path;
    QSettings().setValue(QStringLiteral("appearance/backgroundPath"), path);
    update();
}

void MainWindow::showDeveloperInfo()
{
    QMessageBox::information(this, QStringLiteral("开发者信息"),
        QStringLiteral("作者：Vincinzo\n版本：1.00\n时间：%1\n\n版权所有 © 2026 Vincinzo。保留所有权利。")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
}

void MainWindow::addFile(const QString &path, qint64 size, bool protectedFile)
{
    m_table->setSortingEnabled(false);
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    QTableWidgetItem *check = new QTableWidgetItem;
    check->setCheckState(Qt::Unchecked);
    check->setData(Qt::UserRole, protectedFile);
    check->setFlags(protectedFile ? Qt::ItemIsEnabled : (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable));
    QTableWidgetItem *pathItem = new QTableWidgetItem(path);
    QTableWidgetItem *sizeItem = new QTableWidgetItem(formatBytes(size));
    sizeItem->setData(Qt::UserRole, size);
    QTableWidgetItem *status = new QTableWidgetItem(protectedFile ? QStringLiteral("系统保护：不可删除") : QStringLiteral("可选择"));
    if (protectedFile)
        status->setForeground(QColor("#c2413b"));
    m_table->setItem(row, 0, check);
    m_table->setItem(row, 1, pathItem);
    m_table->setItem(row, 2, sizeItem);
    m_table->setItem(row, 3, status);
    m_table->setSortingEnabled(true);
}

void MainWindow::updateProgress(qint64 visited, qint64 bytes, const QString &path)
{
    m_statusLabel->setText(QStringLiteral("已检查 %1 个文件，发现大文件合计 %2").arg(visited).arg(formatBytes(bytes)));
    m_pathLabel->setText(QStringLiteral("当前：%1").arg(path));
}

void MainWindow::scanFinished(qint64 visited, qint64 matched, qint64 bytes, bool cancelled)
{
    setScanning(false);
    m_pathLabel->clear();
    m_statusLabel->setText(QStringLiteral("%1：检查 %2 个文件，列出 %3 个大文件，共 %4")
                           .arg(cancelled ? QStringLiteral("扫描已停止") : QStringLiteral("扫描完成"))
                           .arg(visited).arg(matched).arg(formatBytes(bytes)));
    if (!cancelled)
        m_trayIcon->showMessage(QStringLiteral("扫描完成"), m_statusLabel->text(), QSystemTrayIcon::Information, 4000);
}

void MainWindow::selectAllSafe(bool checked)
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *item = m_table->item(row, 0);
        if (item && !item->data(Qt::UserRole).toBool())
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

bool MainWindow::moveToRecycleBin(const QString &path, QString *errorMessage)
{
#ifdef Q_OS_WIN
    std::wstring source = QDir::toNativeSeparators(path).toStdWString();
    source.push_back(L'\0');
    source.push_back(L'\0');
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_DELETE;
    op.pFrom = source.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    const int result = SHFileOperationW(&op);
    if (result == 0 && !op.fAnyOperationsAborted)
        return true;
    if (errorMessage)
        *errorMessage = QStringLiteral("Windows 回收站操作失败（代码 %1）").arg(result);
    return false;
#else
    Q_UNUSED(path)
    if (errorMessage) *errorMessage = QStringLiteral("当前平台不支持回收站删除");
    return false;
#endif
}

void MainWindow::deleteSelected()
{
    QStringList paths;
    qint64 total = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *check = m_table->item(row, 0);
        if (check && check->checkState() == Qt::Checked && !check->data(Qt::UserRole).toBool()) {
            paths << m_table->item(row, 1)->text();
            total += m_table->item(row, 2)->data(Qt::UserRole).toLongLong();
        }
    }
    if (paths.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有选中文件"), QStringLiteral("请先勾选要移入回收站的文件。"));
        return;
    }
    const QString preview = paths.mid(0, 8).join(QStringLiteral("\n")) + (paths.size() > 8 ? QStringLiteral("\n……") : QString());
    const auto answer = QMessageBox::warning(this, QStringLiteral("确认移入回收站"),
        QStringLiteral("将把 %1 个文件（共 %2）移入回收站：\n\n%3\n\n请确认这些不是你需要的文件。")
            .arg(paths.size()).arg(formatBytes(total), preview),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    int success = 0;
    QStringList failures;
    for (const QString &path : paths) {
        QString error;
        if (moveToRecycleBin(path, &error))
            ++success;
        else
            failures << QStringLiteral("%1：%2").arg(path, error);
    }
    QMessageBox resultBox(this);
    resultBox.setWindowTitle(QStringLiteral("VIncinzo 磁盘清理完成"));
    resultBox.setIcon(QMessageBox::Information);
    resultBox.setText(QStringLiteral("小帅哥已经帮你清洁干净C盘啦，是不是棒棒的！"));
    resultBox.setInformativeText(
        QStringLiteral("成功移入回收站：%1 个\n失败：%2 个%3")
            .arg(success).arg(failures.size())
            .arg(failures.isEmpty() ? QString() : QStringLiteral("\n\n") + failures.mid(0, 10).join(QStringLiteral("\n"))));
    resultBox.addButton(QStringLiteral("Yes"), QMessageBox::AcceptRole);
    resultBox.addButton(QStringLiteral("Very Yes"), QMessageBox::AcceptRole);
    resultBox.exec();
    refreshDrives();
    startScan();
}

void MainWindow::openSelectedFileLocation()
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 1)) {
        QMessageBox::information(this, QStringLiteral("未选择文件"), QStringLiteral("请先在列表中单击要查看的文件。"));
        return;
    }
    const QString path = m_table->item(row, 1)->text();
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("文件不存在"), QStringLiteral("该文件可能已被移动、删除，或当前无法访问。"));
        return;
    }
    QProcess::startDetached(QStringLiteral("explorer.exe"),
        {QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
}

void MainWindow::showFromTray(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        showNormal();
        raise();
        activateWindow();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_quitting) {
        event->accept();
        return;
    }
    hide();
    m_trayIcon->showMessage(QStringLiteral("VIncinzo 磁盘清理"),
                            QStringLiteral("程序仍在托盘运行，单击托盘图标可重新打开。"),
                            QSystemTrayIcon::Information, 3000);
    event->ignore();
}
