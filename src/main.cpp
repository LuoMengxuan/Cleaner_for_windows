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
        dialog.setWindowTitle(QStringLiteral("欢迎使用 VIncinzo 磁盘清理"));
        dialog.setLabelText(QStringLiteral("第一次见面，请输入作者给你的密码哦。激活后以后就不用再输入啦。"));
        dialog.setTextEchoMode(QLineEdit::Password);
        dialog.setOkButtonText(QStringLiteral("激活"));
        dialog.setCancelButtonText(QStringLiteral("暂不使用"));

        if (dialog.exec() != QDialog::Accepted)
            return false;

        if (dialog.textValue() == QStringLiteral("vincinzo666")) {
            settings.setValue(QStringLiteral("activation/succeeded"), true);
            settings.sync();
            QMessageBox::information(nullptr, QStringLiteral("激活成功"),
                                     QStringLiteral("激活成功！很高兴陪你一起照顾电脑，接下来交给我吧。"));
            return true;
        }

        QMessageBox::warning(nullptr, QStringLiteral("密码还不对哦"),
                             QStringLiteral("别着急，再核对一下作者提供的密码；输入正确后就能开始使用。"));
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
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));

    if (!activateIfNeeded())
        return 0;

    MainWindow window;
    window.show();
    return app.exec();
}
