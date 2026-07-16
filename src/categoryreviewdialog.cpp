#include "categoryreviewdialog.h"
#include "ui_categoryreviewdialog.h"

#include <QColor>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QtGlobal>

namespace {
constexpr int ProtectedFile = 0;
constexpr int DefaultCleanableCache = 1;

const QStringList sectionTitles = {
    QStringLiteral("缓存与临时文件"),
    QStringLiteral("视频与照片"),
    QStringLiteral("文稿、压缩包与其他文件"),
    QStringLiteral("系统文件与软件配置")
};

const QStringList sectionHints = {
    QStringLiteral("这些缓存和临时文件已默认勾选，确认后可放入回收站。"),
    QStringLiteral("照片和视频可能承载重要回忆，请先点击路径查看。"),
    QStringLiteral("文稿、压缩包和其他个人文件需要你主动确认。"),
    QStringLiteral("这些文件会影响系统或软件运行，因此仅供查看。")
};
}

CategoryReviewDialog::CategoryReviewDialog(QTableWidget *sourceTable, QWidget *parent)
    : QDialog(parent), ui(new Ui::CategoryReviewDialog), m_sourceTable(sourceTable),
      m_sectionButtons{nullptr, nullptr, nullptr, nullptr}, m_counts{0, 0, 0, 0}
{
    ui->setupUi(this);
    m_sectionButtons[CacheSection] = ui->cacheButton;
    m_sectionButtons[MediaSection] = ui->mediaButton;
    m_sectionButtons[PersonalSection] = ui->personalButton;
    m_sectionButtons[ProtectedSection] = ui->protectedButton;

    setStyleSheet(QStringLiteral(
        "QDialog { background: #f5f8fb; }"
        "#topBar { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #123d73,stop:1 #168595); border-radius: 12px; }"
        "#shieldLabel { color: #0c6274; background: #dbfbf5; border-radius: 19px; font-size: 22px; font-weight: 800; }"
        "#titleLabel { color: white; font-size: 18px; font-weight: 700; } #summaryLabel { color: #dceff6; font-size: 12px; }"
        "#categorySidebar, #contentPanel { background: white; border: 1px solid #d9e5ee; border-radius: 10px; }"
        "#sideTitle { color: #5d7185; font-size: 12px; font-weight: 700; } #sideHint { color: #7b8d9d; font-size: 11px; }"
        "QToolButton { text-align: left; min-height: 38px; padding: 7px 10px; color: #456176; background: transparent; border: 1px solid transparent; border-radius: 7px; font-weight: 700; }"
        "QToolButton:hover { background: #f0f7fa; } QToolButton:checked { color: #0d647a; background: #e1f4f5; border-color: #9ed4d9; }"
        "#currentSectionTitle { color: #173f61; font-size: 16px; font-weight: 700; } #currentSectionHint { color: #6b7f91; font-size: 12px; }"
        "QTreeWidget { background: #fff; border: 1px solid #d9e6ee; border-radius: 8px; alternate-background-color: #f7fbfd; color: #223b4d; }"
        "QHeaderView::section { background: #eef6f9; color: #42657c; border: none; border-bottom: 1px solid #d8e5ed; padding: 8px; font-weight: 700; }"
        "QTreeWidget::item { padding: 5px; } QTreeWidget::item:selected { background: #ddf0f4; color: #173f61; }"
        "#selectionHint { color: #4a768a; background: #eff9fa; border-radius: 6px; padding: 7px 9px; }"
        "QDialogButtonBox QPushButton { color: white; background: #137a95; border: none; border-radius: 7px; padding: 8px 18px; font-weight: 700; } QDialogButtonBox QPushButton:hover { background: #0d667f; }"));

    ui->categoryTree->setColumnCount(5);
    ui->categoryTree->setHeaderLabels({QStringLiteral("选择"), QStringLiteral("文件类型"), QStringLiteral("文件路径（点击打开）"),
                                       QStringLiteral("大小"), QStringLiteral("处理建议")});
    ui->categoryTree->setRootIsDecorated(false);
    ui->categoryTree->setAlternatingRowColors(true);
    ui->categoryTree->setUniformRowHeights(true);
    ui->categoryTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->categoryTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->categoryTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->categoryTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->categoryTree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->buttonBox->button(QDialogButtonBox::Close)->setText(QStringLiteral("确认选择并返回主界面"));
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);

    for (int section = 0; section < SectionCount; ++section) {
        connect(m_sectionButtons[section], &QToolButton::clicked, this, [this, section] {
            showSection(section);
        });
    }
    connect(ui->categoryTree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        if (column != 0 || !item->flags().testFlag(Qt::ItemIsUserCheckable))
            return;
        const int row = item->data(0, Qt::UserRole).toInt();
        if (m_sourceTable && m_sourceTable->item(row, 0))
            m_sourceTable->item(row, 0)->setCheckState(item->checkState(0));
    });
    connect(ui->categoryTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int column) {
        if (column == 2 && item->flags().testFlag(Qt::ItemIsSelectable))
            emit openFileLocationRequested(item->data(0, Qt::UserRole).toInt());
    });
    populate();
}

CategoryReviewDialog::~CategoryReviewDialog()
{
    delete ui;
}

int CategoryReviewDialog::sectionForRow(int row) const
{
    const int safetyLevel = m_sourceTable->item(row, 0)->data(Qt::UserRole).toInt();
    if (safetyLevel == ProtectedFile)
        return ProtectedSection;
    if (safetyLevel == DefaultCleanableCache)
        return CacheSection;
    const QString category = m_sourceTable->item(row, 3)->text();
    return (category == QStringLiteral("视频") || category == QStringLiteral("图片"))
        ? MediaSection : PersonalSection;
}

void CategoryReviewDialog::populate()
{
    for (int &count : m_counts)
        count = 0;
    if (m_sourceTable) {
        for (int row = 0; row < m_sourceTable->rowCount(); ++row) {
            if (m_sourceTable->item(row, 0))
                ++m_counts[sectionForRow(row)];
        }
    }
    for (int section = 0; section < SectionCount; ++section)
        m_sectionButtons[section]->setText(QStringLiteral("%1  ·  %2").arg(sectionTitles.at(section)).arg(m_counts[section]));
    ui->summaryLabel->setText(QStringLiteral("已自动分组 %1 项文件 · 缓存文件已默认勾选").arg(
        m_counts[CacheSection] + m_counts[MediaSection] + m_counts[PersonalSection] + m_counts[ProtectedSection]));
    showSection(CacheSection);
}

void CategoryReviewDialog::showSection(int section)
{
    if (section < 0 || section >= SectionCount)
        return;
    for (int i = 0; i < SectionCount; ++i) {
        QSignalBlocker blocker(m_sectionButtons[i]);
        m_sectionButtons[i]->setChecked(i == section);
    }
    ui->currentSectionTitle->setText(sectionTitles.at(section));
    ui->currentSectionHint->setText(sectionHints.at(section));
    ui->selectionHint->setText(section == ProtectedSection
        ? QStringLiteral("保护模式：系统与软件配置不提供删除选项。")
        : QStringLiteral("勾选会同步到主界面；点击路径可打开文件位置进行确认。"));

    QSignalBlocker blocker(ui->categoryTree);
    ui->categoryTree->clear();
    if (!m_sourceTable)
        return;
    for (int row = 0; row < m_sourceTable->rowCount(); ++row) {
        if (m_sourceTable->item(row, 0) && sectionForRow(row) == section)
            addFileItem(row);
    }
    if (ui->categoryTree->topLevelItemCount() == 0) {
        QTreeWidgetItem *empty = new QTreeWidgetItem(ui->categoryTree);
        empty->setText(2, QStringLiteral("本次扫描没有此类文件"));
        empty->setFlags(Qt::NoItemFlags);
    }
}

void CategoryReviewDialog::addFileItem(int row)
{
    QTableWidgetItem *check = m_sourceTable->item(row, 0);
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->categoryTree);
    const int safetyLevel = check->data(Qt::UserRole).toInt();
    item->setData(0, Qt::UserRole, row);
    item->setText(1, m_sourceTable->item(row, 3)->text());
    item->setText(2, m_sourceTable->item(row, 1)->text());
    item->setText(3, m_sourceTable->item(row, 2)->text());
    item->setText(4, m_sourceTable->item(row, 4)->text());
    item->setToolTip(2, QStringLiteral("点击打开文件位置：%1").arg(item->text(2)));
    item->setTextAlignment(0, Qt::AlignCenter);
    item->setTextAlignment(1, Qt::AlignCenter);
    item->setTextAlignment(3, Qt::AlignCenter);
    item->setTextAlignment(4, Qt::AlignCenter);
    item->setForeground(2, QColor("#1473a5"));
    if (safetyLevel == ProtectedFile) {
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    } else {
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        item->setCheckState(0, check->checkState());
    }
}
