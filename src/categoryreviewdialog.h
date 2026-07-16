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

    int sectionForRow(int row) const;
    void populate();
    void showSection(int section);
    void addFileItem(int row);

    Ui::CategoryReviewDialog *ui;
    QTableWidget *m_sourceTable;
    QToolButton *m_sectionButtons[SectionCount];
    int m_counts[SectionCount];
};

#endif
