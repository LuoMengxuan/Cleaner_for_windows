#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>

namespace {
bool activateIfNeeded()
{
    QSettings settings;
    if (settings.value(QStringLiteral("activation/succeeded"), false).toBool())
        return true;

    while (true) {
        QInputDialog dialog;
        dialog.setWindowTitle(QStringLiteral("软件激活"));
        dialog.setLabelText(QStringLiteral("请输入作者给你的密码哦。"));
        dialog.setTextEchoMode(QLineEdit::Password);
        dialog.setOkButtonText(QStringLiteral("激活"));
        dialog.setCancelButtonText(QStringLiteral("退出"));

        if (dialog.exec() != QDialog::Accepted)
            return false;

        if (dialog.textValue() == QStringLiteral("vincinzo666")) {
            settings.setValue(QStringLiteral("activation/succeeded"), true);
            settings.sync();
            QMessageBox::information(nullptr, QStringLiteral("激活成功"), QStringLiteral("激活成功"));
            return true;
        }

        QMessageBox::warning(nullptr, QStringLiteral("密码错误"), QStringLiteral("密码错误"));
    }
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("VIncinzo 磁盘清理"));
    app.setOrganizationName(QStringLiteral("HardingWang"));
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(QStringLiteral(":/images/app_icon.png")));

    QFont font(QStringLiteral("Microsoft YaHei UI"), 9);
    app.setFont(font);

    if (!activateIfNeeded())
        return 0;

    MainWindow window;
    window.show();
    return app.exec();
}
