#ifndef CATEGORYREVIEWDIALOG_H
#define CATEGORYREVIEWDIALOG_H

#include <QDialog>

class QTableWidget;
class QToolButton;
class QTreeWidget;

namespace Ui { class CategoryReviewDialog; }

class CategoryReviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CategoryReviewDialog(QTableWidget *sourceTable, QWidget *parent = nullptr);
    ~CategoryReviewDialog() override;

signals:
    void openFileLocationRequested(int row);

private:
    enum Section { CacheSection, MediaSection, PersonalSection, ProtectedSection, SectionCount };

    void setupSection(QToolButton *button, QTreeWidget *tree);
    int sectionForRow(int row) const;
    void populate();

    Ui::CategoryReviewDialog *ui;
    QTableWidget *m_sourceTable;
};

#endif
