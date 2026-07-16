#include "categoryreviewdialog.h"
#include "ui_categoryreviewdialog.h"

#include <QColor>
#include <QDialogButtonBox>
#include <QHeaderView>
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
}
