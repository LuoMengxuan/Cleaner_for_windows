#include "mainwindow.h"
#include "scanner.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStorageInfo>
#include <QStringList>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QProcess>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
class SizeTableWidgetItem : public QTableWidgetItem
{
public:
    explicit SizeTableWidgetItem(const QString &text) : QTableWidgetItem(text) {}

    bool operator<(const QTableWidgetItem &other) const override
    {
        return data(Qt::UserRole).toLongLong() < other.data(Qt::UserRole).toLongLong();
    }
};

struct SelectedFile {
    int row;
    QString path;
    qint64 size;
    bool needsConfirmation;
};

enum FileSafetyLevel {
    ProtectedFile = 0,
    DefaultCleanableCache,
    NeedsUserConfirmation
};

bool isDefaultCleanableCache(const QString &path)
{
    const QString normalized = QDir::toNativeSeparators(path).toLower();
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QStringList cacheFolders = {
        QStringLiteral("\\appdata\\local\\temp\\"),
        QStringLiteral("\\temp\\"),
        QStringLiteral("\\tmp\\"),
        QStringLiteral("\\cache\\"),
        QStringLiteral("\\caches\\"),
        QStringLiteral("\\cache2\\"),
        QStringLiteral("\\crashdumps\\")
    };
    for (const QString &folder : cacheFolders) {
        if (normalized.contains(folder))
            return true;
    }
    return normalized.contains(QStringLiteral("\\users\\")) &&
           (suffix == QStringLiteral("tmp") || suffix == QStringLiteral("temp") ||
            suffix == QStringLiteral("dmp"));
}

FileSafetyLevel safetyLevelFor(const QString &path, bool protectedFile)
{
    if (protectedFile)
        return ProtectedFile;
    return isDefaultCleanableCache(path) ? DefaultCleanableCache : NeedsUserConfirmation;
}

QString safetyDescription(FileSafetyLevel level)
{
    switch (level) {
    case ProtectedFile:
        return QStringLiteral("已保护：系统/软件配置");
    case DefaultCleanableCache:
        return QStringLiteral("默认勾选：缓存可清理");
    case NeedsUserConfirmation:
        return QStringLiteral("请确认：个人/重要文件");
    }
    return QString();
}

QString safetyGroupName(FileSafetyLevel level)
{
    switch (level) {
    case ProtectedFile:
        return QStringLiteral("已保护：系统文件与软件配置（不可删除）");
    case DefaultCleanableCache:
        return QStringLiteral("默认可清理：缓存与临时文件");
    case NeedsUserConfirmation:
        return QStringLiteral("请确认后再处理：视频、照片、文稿与其他个人文件");
    }
    return QString();
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_table(nullptr), m_driveSelector(nullptr),
      m_minimumSize(nullptr), m_scanButton(nullptr), m_stopButton(nullptr), m_deleteButton(nullptr),
      m_openLocationButton(nullptr), m_copyPathButton(nullptr), m_categoryReviewButton(nullptr),
      m_statusLabel(nullptr),
      m_pathLabel(nullptr), m_scanProgressLabel(nullptr), m_scanProgress(nullptr),
      m_driveLayout(nullptr), m_trayIcon(nullptr),
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
    timer->start(30000);
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
    ui->pathLabel->setObjectName(QStringLiteral("pathHint"));
    ui->statusLabel->setObjectName(QStringLiteral("status"));
    ui->driveCard->setObjectName(QStringLiteral("card"));
    ui->guideCard->setObjectName(QStringLiteral("guideCard"));
    ui->guideIcon->setObjectName(QStringLiteral("guideBadge"));
    ui->guideText->setObjectName(QStringLiteral("guideText"));
    ui->safetyLabel->setObjectName(QStringLiteral("safetyHint"));
    ui->scanProgressLabel->setObjectName(QStringLiteral("progressBadge"));
    ui->scanProgressBar->setObjectName(QStringLiteral("scanProgress"));
    ui->scanButton->setObjectName(QStringLiteral("primary"));
    ui->deleteButton->setObjectName(QStringLiteral("danger"));

    m_table = ui->fileTableWidget;
    m_driveSelector = ui->driveComboBox;
    m_minimumSize = ui->minimumSizeComboBox;
    m_scanButton = ui->scanButton;
    m_stopButton = ui->stopButton;
    m_deleteButton = ui->deleteButton;
    m_openLocationButton = ui->openLocationButton;
    m_copyPathButton = ui->copyPathButton;
    m_categoryReviewButton = ui->categoryReviewButton;
    m_statusLabel = ui->statusLabel;
    m_pathLabel = ui->pathLabel;
    m_scanProgressLabel = ui->scanProgressLabel;
    m_scanProgress = ui->scanProgressBar;
    m_driveLayout = ui->driveLayout;

    m_minimumSize->addItem(QStringLiteral("100 MB（推荐）"), 100LL * 1024 * 1024);
    m_minimumSize->addItem(QStringLiteral("500 MB"), 500LL * 1024 * 1024);
    m_minimumSize->addItem(QStringLiteral("1 GB"), 1024LL * 1024 * 1024);
    m_minimumSize->addItem(QStringLiteral("2 GB"), 2LL * 1024 * 1024 * 1024);

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    for (int column : {0, 2, 3, 4}) {
        if (QTableWidgetItem *header = m_table->horizontalHeaderItem(column))
            header->setTextAlignment(Qt::AlignCenter);
    }
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->setSortingEnabled(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    setStyleSheet(QStringLiteral(
        "QMainWindow { background: transparent; }"
        "QWidget#centralWidget { background: transparent; }"
        "QWidget { color: #172b4d; }"
        "#title { font-size: 27px; font-weight: 700; color: #0d3155; }"
        "#subtitle { color: #4d6480; font-size: 13px; }"
        "#card { background: rgba(255, 255, 255, 226); border: 1px solid rgba(209, 225, 239, 230); border-radius: 12px; }"
        "#guideCard { background: rgba(234, 248, 255, 220); border: 1px solid rgba(125, 198, 224, 160); border-radius: 10px; }"
        "#guideBadge { color: #0f5f87; font-weight: 700; padding: 4px 8px; background: rgba(137, 219, 232, 110); border-radius: 8px; }"
        "#guideText { color: #245274; font-size: 13px; }"
        "#safetyHint { color: #60758b; }"
        "#progressBadge { color: #1d5d82; font-weight: 700; min-width: 64px; }"
        "#status { color: #164b76; font-weight: 600; padding: 2px 0; }"
        "#pathHint { color: #61738a; }"
        "QTableWidget { background: rgba(255, 255, 255, 235); border: 1px solid rgba(207, 222, 235, 235); border-radius: 10px; gridline-color: transparent; selection-background-color: #d8f1fb; selection-color: #12314e; }"
        "QTableWidget::item { padding: 4px 6px; border-bottom: 1px solid #edf3f8; }"
        "QHeaderView::section { background: rgba(232, 243, 251, 245); color: #31566f; padding: 9px 7px; border: 0; border-right: 1px solid #dce9f2; font-weight: 700; }"
        "QComboBox, QPushButton { min-height: 31px; padding: 4px 11px; border: 1px solid #b9cede; border-radius: 7px; background: rgba(255, 255, 255, 238); }"
        "QComboBox:hover, QPushButton:hover { background: #f0f9fd; border-color: #78b8d9; }"
        "QPushButton:disabled { color: #92a2af; background: rgba(236, 241, 245, 220); border-color: #d5e0e8; }"
        "QPushButton#primary { color: white; background: #1677a9; border-color: #1677a9; font-weight: 700; }"
        "QPushButton#primary:hover { background: #0f6695; }"
        "QPushButton#danger { color: #b63a45; background: #fff7f7; border-color: #efbcc1; }"
        "QPushButton#danger:hover { background: #fff0f1; border-color: #e08b94; }"
        "QProgressBar { border: 1px solid #c9dce9; border-radius: 6px; background: #edf4f8; text-align: center; min-height: 18px; }"
        "QProgressBar::chunk { background: #3cabc1; border-radius: 5px; }"
        "QCheckBox { spacing: 7px; }"));

    connect(m_scanButton, &QPushButton::clicked, this, &MainWindow::startScan);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopScan);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    connect(m_openLocationButton, &QPushButton::clicked, this, &MainWindow::openSelectedFileLocation);
    connect(m_copyPathButton, &QPushButton::clicked, this, &MainWindow::copySelectedPath);
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::refreshDrives);
    connect(ui->selectAllCheckBox, &QCheckBox::toggled, this, &MainWindow::selectAllSafe);
    connect(ui->filterSafeCheckBox, &QCheckBox::toggled, this, &MainWindow::filterSafeFiles);
    connect(m_categoryReviewButton, &QPushButton::clicked, this, &MainWindow::showCategoryReview);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateSelectionState);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &MainWindow::showFileContextMenu);
    connect(m_driveSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString root = m_driveSelector->currentData().toString();
        m_scanButton->setText(root.isEmpty() ? QStringLiteral("开始扫描")
                                               : QStringLiteral("扫描 %1").arg(QDir::toNativeSeparators(root)));
    });

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助与信息"));
    QAction *guideAction = helpMenu->addAction(QStringLiteral("新手使用指南"));
    QAction *clearAction = helpMenu->addAction(QStringLiteral("清空本次结果"));
    QAction *developerAction = helpMenu->addAction(QStringLiteral("开发者信息"));
    connect(guideAction, &QAction::triggered, this, &MainWindow::showBeginnerGuide);
    connect(clearAction, &QAction::triggered, this, &MainWindow::clearResults);
    connect(developerAction, &QAction::triggered, this, &MainWindow::showDeveloperInfo);
    m_scanProgress->setVisible(false);
    m_scanProgressLabel->setVisible(false);
    updateSelectionState();
}

void MainWindow::setupTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    QMenu *menu = new QMenu(this);
    QAction *showAction = menu->addAction(QStringLiteral("打开 VIncinzo 磁盘清理"));
    QAction *scanAction = menu->addAction(QStringLiteral("扫描当前磁盘"));
    menu->addSeparator();
    QAction *quitAction = menu->addAction(QStringLiteral("退出程序"));
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
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 2).arg(QString::fromLatin1(units[unit]));
}

QString MainWindow::fileCategory(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (isDefaultCleanableCache(path))
        return QStringLiteral("缓存/临时文件");
    if (QStringList({QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
                     QStringLiteral("mov"), QStringLiteral("wmv"), QStringLiteral("flv")}).contains(suffix))
        return QStringLiteral("视频");
    if (QStringList({QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"),
                     QStringLiteral("tar"), QStringLiteral("gz")}).contains(suffix))
        return QStringLiteral("压缩包");
    if (QStringList({QStringLiteral("iso"), QStringLiteral("img"), QStringLiteral("vhd"),
                     QStringLiteral("vhdx")}).contains(suffix))
        return QStringLiteral("镜像文件");
    if (QStringList({QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
                     QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp")}).contains(suffix))
        return QStringLiteral("图片");
    if (QStringList({QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("xls"),
                     QStringLiteral("xlsx"), QStringLiteral("ppt"), QStringLiteral("pptx"),
                     QStringLiteral("pdf")}).contains(suffix))
        return QStringLiteral("文档");
    if (QStringList({QStringLiteral("bak"), QStringLiteral("db"), QStringLiteral("sql"),
                     QStringLiteral("log"), QStringLiteral("tmp")}).contains(suffix))
        return QStringLiteral("数据/备份");
    if (QStringList({QStringLiteral("exe"), QStringLiteral("msi"), QStringLiteral("dll"),
                     QStringLiteral("sys")}).contains(suffix))
        return QStringLiteral("程序文件");
    return suffix.isEmpty() ? QStringLiteral("其他") : QStringLiteral("%1 文件").arg(suffix.toUpper());
}

QIcon MainWindow::makeTrayIcon(int usedPercent)
{
    QPixmap pixmap = QIcon(QStringLiteral(":/images/app_icon.png")).pixmap(64, 64);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color = usedPercent >= 90 ? QColor("#e5484d")
                       : usedPercent >= 75 ? QColor("#e89a26") : QColor("#1677a9");
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(color);
    painter.drawEllipse(34, 35, 28, 28);
    painter.setPen(Qt::white);
    painter.setFont(QFont(QStringLiteral("Arial"), 7, QFont::Bold));
    painter.drawText(QRect(34, 35, 28, 28), Qt::AlignCenter, QString::number(usedPercent));
    return QIcon(pixmap);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPixmap source(QStringLiteral(":/images/background_professional.png"));
    if (!source.isNull()) {
        const QPixmap cover = source.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int x = (cover.width() - width()) / 2;
        const int y = (cover.height() - height()) / 2;
        painter.drawPixmap(rect(), cover, QRect(x, y, width(), height()));
    }
    painter.fillRect(rect(), QColor(244, 250, 253, 48));
}

void MainWindow::refreshDrives()
{
    if (m_scanning)
        return;

    while (QLayoutItem *item = m_driveLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    QStringList tips;
    int cUsed = 0;
    qint64 cAvailable = 0;
    qint64 cTotal = 0;
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
        const QString label = drive.displayName().isEmpty() ? root
            : QStringLiteral("%1  %2").arg(root, drive.displayName());
        m_driveSelector->addItem(label, drive.rootPath());

        QWidget *row = new QWidget;
        QHBoxLayout *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 2, 0, 2);
        QLabel *name = new QLabel(QStringLiteral("%1  可用 %2 / %3")
                                      .arg(root, formatBytes(drive.bytesAvailable()), formatBytes(drive.bytesTotal())));
        name->setMinimumWidth(290);
        QProgressBar *bar = new QProgressBar;
        bar->setRange(0, 100);
        bar->setValue(percent);
        bar->setFormat(QStringLiteral("已使用 %1%").arg(percent));
        layout->addWidget(name);
        layout->addWidget(bar, 1);
        m_driveLayout->addWidget(row);

        tips << QStringLiteral("%1 可用 %2 / %3").arg(root, formatBytes(drive.bytesAvailable()), formatBytes(drive.bytesTotal()));
        if (root.startsWith(QStringLiteral("C:"), Qt::CaseInsensitive)) {
            cUsed = percent;
            cAvailable = drive.bytesAvailable();
            cTotal = drive.bytesTotal();
        }
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
    m_scanButton->setText(selectedRoot.isEmpty() ? QStringLiteral("开始扫描")
                                                   : QStringLiteral("扫描 %1").arg(QDir::toNativeSeparators(selectedRoot)));
    m_trayIcon->setIcon(makeTrayIcon(cUsed));
    m_trayIcon->setToolTip(QStringLiteral("VIncinzo 磁盘清理\n") + tips.join(QStringLiteral("\n")));
    if (m_driveSelector->count() == 0)
        m_statusLabel->setText(QStringLiteral("暂时没有发现可访问的磁盘，检查一下设备连接后再试试。"));
    else if (m_table->rowCount() == 0 && cTotal > 0) {
        if (cUsed >= 90)
            m_pathLabel->setText(QStringLiteral("C 盘可用 %1，空间有点紧张；从大文件开始检查会更有效。")
                                     .arg(formatBytes(cAvailable)));
        else if (cUsed >= 75)
            m_pathLabel->setText(QStringLiteral("C 盘可用 %1，空间尚可；偶尔整理一下会更安心。")
                                     .arg(formatBytes(cAvailable)));
        else
            m_pathLabel->setText(QStringLiteral("C 盘可用 %1，状态不错；你的电脑被照顾得很好。")
                                     .arg(formatBytes(cAvailable)));
    }
}

void MainWindow::startScan()
{
    if (m_scanning)
        return;

    const QString rootPath = m_driveSelector->currentData().toString();
    if (rootPath.isEmpty()) {
        showFriendlyMessage(QStringLiteral("还差一步"),
                            QStringLiteral("先选择一个可访问的磁盘吧，我会帮你从大文件开始慢慢检查。"));
        return;
    }

    m_table->setSortingEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(0);
    resetSelectionUi();
    m_scanProgressLabel->setText(QStringLiteral("扫描进行中"));
    m_scanProgress->setRange(0, 0);
    m_scanProgress->setFormat(QStringLiteral("正在扫描，请稍候……"));
    m_scanProgressLabel->setVisible(true);
    m_scanProgress->setVisible(true);
    m_pathLabel->setText(QStringLiteral("正在准备扫描……"));
    m_statusLabel->setText(QStringLiteral("正在检查 %1 的大文件，请稍等；你已经做得很棒了。")
                               .arg(QDir::toNativeSeparators(rootPath)));
    setScanning(true);
    emit requestScan(rootPath, m_minimumSize->currentData().toLongLong());
}

void MainWindow::stopScan()
{
    if (!m_scanning)
        return;
    emit requestCancel();
    m_scanProgressLabel->setText(QStringLiteral("正在安全停止"));
    m_scanProgress->setFormat(QStringLiteral("正在保存当前结果……"));
    m_statusLabel->setText(QStringLiteral("正在停在安全的位置，请稍等一下……"));
}

void MainWindow::setScanning(bool active)
{
    m_scanning = active;
    m_scanButton->setEnabled(!active);
    m_stopButton->setEnabled(active);
    m_minimumSize->setEnabled(!active);
    m_driveSelector->setEnabled(!active);
    ui->refreshButton->setEnabled(!active);
    ui->selectAllCheckBox->setEnabled(!active);
    ui->filterSafeCheckBox->setEnabled(!active);
    m_categoryReviewButton->setEnabled(!active);
    m_table->setEnabled(!active);
    if (active) {
        m_deleteButton->setEnabled(false);
        m_openLocationButton->setEnabled(false);
        m_copyPathButton->setEnabled(false);
    } else {
        updateSelectionState();
    }
}

void MainWindow::showBeginnerGuide()
{
    showFriendlyMessage(QStringLiteral("三步完成一次安心清理"),
        QStringLiteral("1. 选择想检查的磁盘，点击“扫描”。\n"
                       "2. 在结果中点“打开文件位置”，先确认它确实不再需要。\n"
                       "3. 勾选后放入回收站；如果反悔，还能从回收站恢复。"),
        QStringLiteral("小提醒：系统文件和常见程序文件会被保护，只展示，不会让你误删。"));
}

void MainWindow::showDeveloperInfo()
{
    showFriendlyMessage(QStringLiteral("开发者信息"),
        QStringLiteral("作者：Vincinzo\n版本：1.21\nGit 版本：v1.2.1\n时间：%1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))),
        QStringLiteral("Copyright © 2026 Vincinzo. 保留所有权利。\n谢谢你认真照顾自己的电脑。"));
}

void MainWindow::addFile(const QString &path, qint64 size, bool protectedFile)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    const FileSafetyLevel level = safetyLevelFor(path, protectedFile);

    QTableWidgetItem *check = new QTableWidgetItem;
    check->setCheckState(level == DefaultCleanableCache ? Qt::Checked : Qt::Unchecked);
    check->setTextAlignment(Qt::AlignCenter);
    check->setData(Qt::UserRole, static_cast<int>(level));
    check->setToolTip(level == ProtectedFile
                           ? QStringLiteral("为保护电脑稳定性，此类文件只展示，不提供删除操作。")
                           : level == DefaultCleanableCache
                               ? QStringLiteral("这是缓存或临时文件，默认已勾选；仍会先放入回收站。")
                               : QStringLiteral("这可能是你的个人文件，请先打开位置确认后再决定是否放入回收站。"));
    check->setFlags(level == ProtectedFile ? Qt::ItemIsEnabled
                                  : (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable));

    QTableWidgetItem *pathItem = new QTableWidgetItem(QDir::toNativeSeparators(path));
    pathItem->setToolTip(QDir::toNativeSeparators(path));
    SizeTableWidgetItem *sizeItem = new SizeTableWidgetItem(formatBytes(size));
    sizeItem->setData(Qt::UserRole, size);
    QTableWidgetItem *category = new QTableWidgetItem(fileCategory(path));
    QTableWidgetItem *status = new QTableWidgetItem(safetyDescription(level));
    status->setForeground(level == ProtectedFile ? QColor("#b45359")
                          : level == DefaultCleanableCache ? QColor("#19705c") : QColor("#9a6700"));
    sizeItem->setTextAlignment(Qt::AlignCenter);
    category->setTextAlignment(Qt::AlignCenter);
    status->setTextAlignment(Qt::AlignCenter);

    m_table->setItem(row, 0, check);
    m_table->setItem(row, 1, pathItem);
    m_table->setItem(row, 2, sizeItem);
    m_table->setItem(row, 3, category);
    m_table->setItem(row, 4, status);
    if (ui->filterSafeCheckBox->isChecked() && level != DefaultCleanableCache)
        m_table->setRowHidden(row, true);
}

void MainWindow::updateProgress(qint64 visited, qint64 bytes, const QString &path)
{
    m_scanProgress->setFormat(QStringLiteral("已检查 %1 个文件").arg(visited));
    m_statusLabel->setText(QStringLiteral("已经检查 %1 个文件，发现大文件合计 %2。")
                               .arg(visited).arg(formatBytes(bytes)));
    m_pathLabel->setText(QStringLiteral("当前检查：%1").arg(QDir::toNativeSeparators(path)));
}

void MainWindow::scanFinished(qint64 visited, qint64 matched, qint64 bytes, bool cancelled)
{
    setScanning(false);
    m_scanProgress->setRange(0, 100);
    m_scanProgress->setValue(cancelled ? 0 : 100);
    m_scanProgress->setFormat(cancelled ? QStringLiteral("扫描已停止") : QStringLiteral("扫描完成"));
    m_scanProgressLabel->setText(cancelled ? QStringLiteral("扫描已停止") : QStringLiteral("扫描完成"));
    m_pathLabel->clear();
    m_table->setSortingEnabled(true);
    if (matched > 0)
        m_table->sortItems(2, Qt::DescendingOrder);

    if (cancelled) {
        m_statusLabel->setText(QStringLiteral("扫描已暂停，已为你保留当前找到的 %1 个大文件。")
                                   .arg(matched));
        QTimer::singleShot(2600, this, [this] {
            if (!m_scanning) {
                m_scanProgress->setVisible(false);
                m_scanProgressLabel->setVisible(false);
            }
        });
        return;
    }

    m_statusLabel->setText(QStringLiteral("扫描完成：检查 %1 个文件，找到 %2 个大文件，共 %3。")
                               .arg(visited).arg(matched).arg(formatBytes(bytes)));
    m_trayIcon->showMessage(QStringLiteral("扫描完成"), m_statusLabel->text(), QSystemTrayIcon::Information, 5000);
    if (matched == 0) {
        showFriendlyMessage(QStringLiteral("太棒了，空间很整洁"),
            QStringLiteral("按照当前大小条件，没有发现需要处理的大文件。你的磁盘状态不错，继续保持就好！"));
    } else {
        m_pathLabel->setText(QStringLiteral("已默认勾选缓存文件；请在分类窗口中确认视频、照片和文稿等个人内容。"));
        QTimer::singleShot(0, this, &MainWindow::showCategoryReview);
    }
    QTimer::singleShot(2600, this, [this] {
        if (!m_scanning) {
            m_scanProgress->setVisible(false);
            m_scanProgressLabel->setVisible(false);
        }
    });
}

void MainWindow::selectAllSafe(bool checked)
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *item = m_table->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == DefaultCleanableCache)
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

void MainWindow::resetSelectionUi()
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *item = m_table->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() != ProtectedFile)
            item->setCheckState(Qt::Unchecked);
    }
    m_table->clearSelection();
    ui->selectAllCheckBox->blockSignals(true);
    ui->selectAllCheckBox->setChecked(false);
    ui->selectAllCheckBox->blockSignals(false);
}

void MainWindow::filterSafeFiles(bool enabled)
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *item = m_table->item(row, 0);
        const bool isDefaultCache = item && item->data(Qt::UserRole).toInt() == DefaultCleanableCache;
        m_table->setRowHidden(row, enabled && !isDefaultCache);
    }
    m_statusLabel->setText(enabled
        ? QStringLiteral("现在只显示默认可清理的缓存文件，其他内容需要你主动确认。")
        : QStringLiteral("已显示全部扫描结果；系统和软件配置文件仍然不能被删除。"));
}

void MainWindow::showCategoryReview()
{
    if (m_scanning)
        return;
    if (m_table->rowCount() == 0) {
        showFriendlyMessage(QStringLiteral("还没有扫描结果"),
            QStringLiteral("先完成一次扫描，我就能按缓存、个人文件和受保护文件帮你整理。"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("分类查看与确认"));
    dialog.setModal(true);
    dialog.resize(1080, 610);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *intro = new QLabel(QStringLiteral(
        "缓存与临时文件已默认勾选；视频、照片、文稿和压缩包需要你主动确认。"
        "<br><b>点击任意文件路径</b>即可打开它所在的位置，系统和软件配置文件始终不可勾选。"), &dialog);
    intro->setWordWrap(true);
    intro->setStyleSheet(QStringLiteral("QLabel { color: #245274; padding: 8px 10px; background: #edf8fb; border-radius: 8px; }"));
    layout->addWidget(intro);

    QTreeWidget tree(&dialog);
    tree.setColumnCount(5);
    tree.setHeaderLabels({QStringLiteral("选择"), QStringLiteral("分类"), QStringLiteral("文件路径（点击打开）"),
                          QStringLiteral("大小"), QStringLiteral("处理建议")});
    tree.setRootIsDecorated(true);
    tree.setAlternatingRowColors(true);
    tree.setUniformRowHeights(true);
    tree.header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree.header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree.header()->setSectionResizeMode(2, QHeaderView::Stretch);
    tree.header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tree.header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    layout->addWidget(&tree, 1);

    QTreeWidgetItem *groups[3] = {nullptr, nullptr, nullptr};
    {
        QSignalBlocker blocker(&tree);
        for (int row = 0; row < m_table->rowCount(); ++row) {
            QTableWidgetItem *check = m_table->item(row, 0);
            if (!check)
                continue;
            const FileSafetyLevel level = static_cast<FileSafetyLevel>(check->data(Qt::UserRole).toInt());
            const int groupIndex = static_cast<int>(level);
            if (!groups[groupIndex]) {
                groups[groupIndex] = new QTreeWidgetItem(&tree);
                groups[groupIndex]->setText(0, safetyGroupName(level));
                groups[groupIndex]->setData(0, Qt::UserRole, groupIndex);
                groups[groupIndex]->setFirstColumnSpanned(true);
                if (level == ProtectedFile) {
                    groups[groupIndex]->setFlags(Qt::ItemIsEnabled);
                } else {
                    groups[groupIndex]->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
                    groups[groupIndex]->setCheckState(0, level == DefaultCleanableCache ? Qt::Checked : Qt::Unchecked);
                }
            }

            QTreeWidgetItem *child = new QTreeWidgetItem(groups[groupIndex]);
            child->setData(0, Qt::UserRole, row);
            child->setText(1, m_table->item(row, 3)->text());
            child->setText(2, m_table->item(row, 1)->text());
            child->setText(3, m_table->item(row, 2)->text());
            child->setText(4, m_table->item(row, 4)->text());
            child->setToolTip(2, QStringLiteral("点击打开文件位置：%1").arg(m_table->item(row, 1)->text()));
            child->setTextAlignment(0, Qt::AlignCenter);
            child->setTextAlignment(1, Qt::AlignCenter);
            child->setTextAlignment(3, Qt::AlignCenter);
            child->setTextAlignment(4, Qt::AlignCenter);
            child->setForeground(2, QColor("#1473a5"));
            if (level == ProtectedFile) {
                child->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            } else {
                child->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
                child->setCheckState(0, check->checkState());
            }
        }
    }

    if (groups[DefaultCleanableCache])
        tree.expandItem(groups[DefaultCleanableCache]);

    connect(&tree, &QTreeWidget::itemChanged, &dialog, [this, &tree](QTreeWidgetItem *item, int column) {
        if (column != 0)
            return;
        if (!item->parent()) {
            const FileSafetyLevel level = static_cast<FileSafetyLevel>(item->data(0, Qt::UserRole).toInt());
            if (level == ProtectedFile || item->checkState(0) == Qt::PartiallyChecked)
                return;
            QSignalBlocker blocker(&tree);
            for (int i = 0; i < item->childCount(); ++i) {
                QTreeWidgetItem *child = item->child(i);
                child->setCheckState(0, item->checkState(0));
                const int row = child->data(0, Qt::UserRole).toInt();
                if (QTableWidgetItem *tableCheck = m_table->item(row, 0))
                    tableCheck->setCheckState(item->checkState(0));
            }
            return;
        }

        const int row = item->data(0, Qt::UserRole).toInt();
        if (QTableWidgetItem *tableCheck = m_table->item(row, 0))
            tableCheck->setCheckState(item->checkState(0));

        QTreeWidgetItem *parent = item->parent();
        int checked = 0;
        for (int i = 0; i < parent->childCount(); ++i) {
            if (parent->child(i)->checkState(0) == Qt::Checked)
                ++checked;
        }
        QSignalBlocker blocker(&tree);
        parent->setCheckState(0, checked == 0 ? Qt::Unchecked
                                  : checked == parent->childCount() ? Qt::Checked : Qt::PartiallyChecked);
    });

    connect(&tree, &QTreeWidget::itemClicked, &dialog, [this](QTreeWidgetItem *item, int column) {
        if (!item->parent() || column != 2)
            return;
        const int row = item->data(0, Qt::UserRole).toInt();
        m_table->setCurrentCell(row, 1);
        openSelectedFileLocation();
    });

    QDialogButtonBox buttons(QDialogButtonBox::Close, &dialog);
    QPushButton *closeButton = buttons.button(QDialogButtonBox::Close);
    closeButton->setText(QStringLiteral("确认选择，返回主界面"));
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(&buttons);
    dialog.exec();
    updateSelectionState();
}

void MainWindow::showFileContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = m_table->itemAt(pos);
    if (!item)
        return;
    m_table->setCurrentCell(item->row(), item->column());

    QMenu menu(this);
    QAction *openAction = menu.addAction(QStringLiteral("打开文件位置"));
    QAction *copyAction = menu.addAction(QStringLiteral("复制完整路径"));
    QAction *selected = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (selected == openAction)
        openSelectedFileLocation();
    else if (selected == copyAction)
        copySelectedPath();
}

void MainWindow::clearResults()
{
    if (m_scanning)
        return;
    m_table->setSortingEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(0);
    m_table->setSortingEnabled(true);
    resetSelectionUi();
    m_pathLabel->clear();
    m_statusLabel->setText(QStringLiteral("本次结果已清空。需要时重新扫描就好，不用着急。"));
    updateSelectionState();
}

void MainWindow::updateSelectionState()
{
    const int row = m_table ? m_table->currentRow() : -1;
    const bool hasSelection = row >= 0 && m_table->item(row, 1);
    const bool canDelete = hasSelection && m_table->item(row, 0) &&
                           m_table->item(row, 0)->data(Qt::UserRole).toInt() != ProtectedFile;
    m_openLocationButton->setEnabled(!m_scanning && hasSelection);
    m_copyPathButton->setEnabled(!m_scanning && hasSelection);
    m_deleteButton->setEnabled(!m_scanning && (canDelete || m_table->rowCount() > 0));
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
        *errorMessage = QStringLiteral("Windows 回收站操作未完成（代码 %1）。").arg(result);
    return false;
#else
    Q_UNUSED(path)
    if (errorMessage)
        *errorMessage = QStringLiteral("当前系统暂不支持移入回收站。");
    return false;
#endif
}

void MainWindow::deleteSelected()
{
    QVector<SelectedFile> selected;
    qint64 total = 0;
    int needsConfirmationCount = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *check = m_table->item(row, 0);
        if (check && check->checkState() == Qt::Checked &&
            check->data(Qt::UserRole).toInt() != ProtectedFile) {
            const QString path = m_table->item(row, 1)->text();
            const qint64 size = m_table->item(row, 2)->data(Qt::UserRole).toLongLong();
            const bool needsConfirmation = check->data(Qt::UserRole).toInt() == NeedsUserConfirmation;
            selected.push_back({row, path, size, needsConfirmation});
            total += size;
            if (needsConfirmation)
                ++needsConfirmationCount;
        }
    }

    if (selected.isEmpty()) {
        showFriendlyMessage(QStringLiteral("先选一选吧"),
            QStringLiteral("请勾选确认不再需要的文件。我会把它们放进回收站，不会直接彻底删除。"));
        return;
    }

    QStringList preview;
    for (int i = 0; i < selected.size() && i < 5; ++i)
        preview << selected.at(i).path;

    QMessageBox confirmation(this);
    confirmation.setWindowTitle(QStringLiteral("最后确认一下"));
    confirmation.setIcon(QMessageBox::Question);
    confirmation.setText(QStringLiteral("准备将 %1 个文件（约 %2）放入回收站。")
                             .arg(selected.size()).arg(formatBytes(total)));
    const QString reviewReminder = needsConfirmationCount > 0
        ? QStringLiteral("其中有 %1 个视频、照片、文稿或其他个人文件；请确认它们不是你要保留的内容。\n\n")
              .arg(needsConfirmationCount)
        : QString();
    confirmation.setInformativeText(QStringLiteral("%1请确认这些文件确实不再需要。即使放进回收站，暂时也还能恢复。\n\n%2%3")
        .arg(reviewReminder, preview.join(QStringLiteral("\n")),
             selected.size() > 5 ? QStringLiteral("\n……") : QString()));
    QPushButton *moveButton = confirmation.addButton(QStringLiteral("放心放入回收站"), QMessageBox::AcceptRole);
    confirmation.addButton(QStringLiteral("我再看看"), QMessageBox::RejectRole);
    confirmation.exec();
    if (confirmation.clickedButton() != moveButton)
        return;

    int success = 0;
    QStringList failures;
    QVector<int> successRows;
    for (const SelectedFile &file : selected) {
        QString error;
        if (moveToRecycleBin(file.path, &error)) {
            ++success;
            successRows << file.row;
        } else {
            failures << QStringLiteral("%1：%2").arg(file.path, error);
        }
    }

    std::sort(successRows.begin(), successRows.end(), std::greater<int>());
    const bool wasSorting = m_table->isSortingEnabled();
    m_table->setSortingEnabled(false);
    for (int row : successRows)
        m_table->removeRow(row);
    m_table->setSortingEnabled(wasSorting);

    resetSelectionUi();
    refreshDrives();
    updateSelectionState();
    QString message;
    if (success > 0)
        message = QStringLiteral("干得漂亮！我已经帮你把 %1 个文件放进回收站了，暂时释放约 %2 空间。")
                      .arg(success).arg(formatBytes(total));
    else
        message = QStringLiteral("这次没有成功移动文件，别着急，我们可以先检查文件是否仍被其他程序占用。");

    QString details;
    if (failures.isEmpty())
        details = QStringLiteral("你做得很谨慎，这正是安全清理最重要的一步。");
    else
        details = QStringLiteral("以下文件暂未处理：\n%1").arg(failures.mid(0, 8).join(QStringLiteral("\n")));
    showFriendlyMessage(QStringLiteral("清理完成"), message, details);
}

void MainWindow::openSelectedFileLocation()
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 1)) {
        showFriendlyMessage(QStringLiteral("先选择一个文件"),
            QStringLiteral("在列表中点一下想查看的文件，我就带你打开它所在的位置。"));
        return;
    }

    const QString path = m_table->item(row, 1)->text();
    if (!QFileInfo::exists(path)) {
        showFriendlyMessage(QStringLiteral("文件已经不在原处"),
            QStringLiteral("这个文件可能已被移动、删除，或暂时无法访问。你可以重新扫描一次。"));
        return;
    }
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
}

void MainWindow::copySelectedPath()
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 1)) {
        showFriendlyMessage(QStringLiteral("先选择一个文件"),
            QStringLiteral("点一下列表中的文件后，就可以复制它的完整路径。"));
        return;
    }
    const QString path = m_table->item(row, 1)->text();
    QApplication::clipboard()->setText(path);
    m_statusLabel->setText(QStringLiteral("路径已复制，你做事很细心，先确认再清理是最好的习惯。"));
}

void MainWindow::showFriendlyMessage(const QString &title, const QString &message, const QString &details)
{
    QMessageBox box(this);
    box.setWindowTitle(title);
    box.setIcon(QMessageBox::Information);
    box.setText(message);
    if (!details.isEmpty())
        box.setInformativeText(details);
    box.addButton(QStringLiteral("知道啦"), QMessageBox::AcceptRole);
    box.exec();
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
    if (m_quitting || !QSystemTrayIcon::isSystemTrayAvailable()) {
        event->accept();
        return;
    }
    hide();
    m_trayIcon->showMessage(QStringLiteral("VIncinzo 磁盘清理"),
                            QStringLiteral("我还在托盘里待命，需要时点一下图标就能继续使用。"),
                            QSystemTrayIcon::Information, 3500);
    event->ignore();
}
