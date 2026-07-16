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
    QStringLiteral("默认可清理 · 缓存与临时文件"),
    QStringLiteral("请确认 · 视频与照片"),
    QStringLiteral("请确认 · 文稿、压缩包与其他文件"),
    QStringLiteral("已保护 · 系统文件与软件配置")
};
}

CategoryReviewDialog::CategoryReviewDialog(QTableWidget *sourceTable, QWidget *parent)
    : QDialog(parent), ui(new Ui::CategoryReviewDialog), m_sourceTable(sourceTable)
{
    ui->setupUi(this);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #f4f7fb; }"
        "#heroFrame { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #123e73, stop:1 #16879c); border: 0; border-radius: 14px; }"
        "#shieldLabel { color: #0c5e72; background: #d9fbf5; border-radius: 26px; font-size: 30px; font-weight: 800; }"
        "#titleLabel { color: white; font-size: 21px; font-weight: 700; }"
        "#introLabel { color: #dceef8; font-size: 12px; }"
        "#statusBadge { color: #d9fbf5; background: rgba(4, 42, 75, 105); border: 1px solid rgba(220, 255, 249, 125); border-radius: 12px; padding: 7px 11px; font-weight: 700; }"
        "#cacheSummaryCard, #reviewSummaryCard, #protectedSummaryCard { background: white; border-radius: 10px; }"
        "#cacheSummaryCard { border: 1px solid #bfe8d8; } #reviewSummaryCard { border: 1px solid #f0d7aa; } #protectedSummaryCard { border: 1px solid #c9d9ed; }"
        "#cacheSummaryTitle { color: #29795e; font-weight: 700; } #reviewSummaryTitle { color: #a56608; font-weight: 700; } #protectedSummaryTitle { color: #426987; font-weight: 700; }"
        "#cacheCountLabel, #reviewCountLabel, #protectedCountLabel { color: #1e3549; font-size: 17px; font-weight: 700; }"
        "QToolButton { text-align: left; padding: 10px 14px; font-size: 14px; font-weight: 700; border-radius: 9px; }"
        "QToolButton#cacheButton { color: #226b54; background: #ecfbf4; border: 1px solid #b9e7d5; } QToolButton#cacheButton:checked { background: #d9f5e7; border-color: #62bb97; }"
        "QToolButton#mediaButton { color: #8a5b1a; background: #fff7e8; border: 1px solid #f0d39b; } QToolButton#mediaButton:checked { background: #ffefd1; border-color: #d9a94b; }"
        "QToolButton#personalButton { color: #64499b; background: #f5f0ff; border: 1px solid #d6c5f0; } QToolButton#personalButton:checked { background: #eadeff; border-color: #a78bd4; }"
        "QToolButton#protectedButton { color: #476985; background: #eef4fa; border: 1px solid #c5d9ec; } QToolButton#protectedButton:checked { background: #e0edf8; border-color: #7ca6ca; }"
        "QTreeWidget { background: white; border: 1px solid #d9e5ef; border-radius: 8px; alternate-background-color: #f7fbfe; color: #21384b; }"
        "QHeaderView::section { background: #eaf3f8; border: none; border-bottom: 1px solid #d6e4ee; color: #4b667b; padding: 7px; font-weight: 700; }"
        "QTreeWidget::item { padding: 4px; } QTreeWidget::item:selected { background: #d9edf6; color: #123e5f; }"
        "QScrollBar:vertical { width: 10px; background: transparent; margin: 4px; } QScrollBar::handle:vertical { background: #a5c8d7; min-height: 34px; border-radius: 5px; }"
        "QDialogButtonBox QPushButton { color: white; background: #147a9b; border: none; border-radius: 7px; padding: 8px 18px; font-weight: 700; } QDialogButtonBox QPushButton:hover { background: #0e6685; }"));
    ui->summaryLayout->setStretch(0, 1);
    ui->summaryLayout->setStretch(1, 1);
    ui->summaryLayout->setStretch(2, 1);
    ui->buttonBox->button(QDialogButtonBox::Close)->setText(QStringLiteral("确认选择并返回主界面"));
    setupSection(ui->cacheButton, ui->cacheTree);
    setupSection(ui->mediaButton, ui->mediaTree);
    setupSection(ui->personalButton, ui->personalTree);
    setupSection(ui->protectedButton, ui->protectedTree);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    populate();
}

CategoryReviewDialog::~CategoryReviewDialog()
{
    delete ui;
}

void CategoryReviewDialog::setupSection(QToolButton *button, QTreeWidget *tree)
{
    tree->setColumnCount(5);
    tree->setHeaderLabels({QStringLiteral("选择"), QStringLiteral("文件类型"), QStringLiteral("文件路径（点击打开）"),
                           QStringLiteral("大小"), QStringLiteral("处理建议")});
    tree->setRootIsDecorated(false);
    tree->setAlternatingRowColors(true);
    tree->setUniformRowHeights(true);
    tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tree->setVisible(false);

    connect(button, &QToolButton::toggled, this, [button, tree](bool expanded) {
        button->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        tree->setVisible(expanded);
    });
    connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        if (column != 0 || !item->flags().testFlag(Qt::ItemIsUserCheckable))
            return;
        const int row = item->data(0, Qt::UserRole).toInt();
        if (m_sourceTable && m_sourceTable->item(row, 0))
            m_sourceTable->item(row, 0)->setCheckState(item->checkState(0));
    });
    connect(tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int column) {
        if (column == 2 && item->flags().testFlag(Qt::ItemIsSelectable))
            emit openFileLocationRequested(item->data(0, Qt::UserRole).toInt());
    });
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
    QTreeWidget *trees[SectionCount] = {
        ui->cacheTree, ui->mediaTree, ui->personalTree, ui->protectedTree
    };
    QToolButton *buttons[SectionCount] = {
        ui->cacheButton, ui->mediaButton, ui->personalButton, ui->protectedButton
    };
    int counts[SectionCount] = {0, 0, 0, 0};

    for (QTreeWidget *tree : trees) {
        QSignalBlocker blocker(tree);
        tree->clear();
    }

    if (m_sourceTable) {
        for (int row = 0; row < m_sourceTable->rowCount(); ++row) {
            QTableWidgetItem *check = m_sourceTable->item(row, 0);
            if (!check)
                continue;
            const int section = sectionForRow(row);
            QTreeWidgetItem *item = new QTreeWidgetItem(trees[section]);
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
            ++counts[section];
        }
    }

    for (int section = 0; section < SectionCount; ++section) {
        QTreeWidget *tree = trees[section];
        if (counts[section] == 0) {
            QTreeWidgetItem *empty = new QTreeWidgetItem(tree);
            empty->setText(2, QStringLiteral("本次扫描没有此类文件"));
            empty->setFlags(Qt::NoItemFlags);
        }
        buttons[section]->setText(QStringLiteral("%1  ·  %2 项").arg(sectionTitles.at(section)).arg(counts[section]));
        tree->setFixedHeight(qMin(330, 34 + qMax(1, tree->topLevelItemCount()) * 29));
    }

    const int reviewCount = counts[MediaSection] + counts[PersonalSection];
    ui->cacheCountLabel->setText(QStringLiteral("%1 项 已默认勾选").arg(counts[CacheSection]));
    ui->reviewCountLabel->setText(QStringLiteral("%1 项 等待确认").arg(reviewCount));
    ui->protectedCountLabel->setText(QStringLiteral("%1 项 已锁定保护").arg(counts[ProtectedSection]));
    ui->statusBadge->setText(QStringLiteral("● 已分类 %1 项文件").arg(
        counts[CacheSection] + reviewCount + counts[ProtectedSection]));
}
