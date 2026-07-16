#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QtGlobal>

namespace {
const QString kActivationAlphabet = QStringLiteral("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");

QString activationChecksum(const QString &payload)
{
    quint32 state = 0x5A17C3D9u;
    for (const QChar character : payload) {
        state ^= static_cast<quint32>(character.unicode());
        state *= 16777619u;
        state ^= state >> 13;
        state *= 0x85EBCA6Bu;
    }

    QString checksum;
    for (int i = 0; i < 4; ++i) {
        checksum.prepend(kActivationAlphabet.at(static_cast<int>(state % kActivationAlphabet.size())));
        state = (state >> 7) ^ (state * 0x27D4EB2Du) ^ static_cast<quint32>(i * 97 + 31);
    }
    return checksum;
}

bool isValidActivationCode(QString code)
{
    code = code.trimmed().toUpper();
    if (code.size() != 10)
        return false;

    for (const QChar character : code) {
        if (!kActivationAlphabet.contains(character))
            return false;
    }

    return code.right(4) == activationChecksum(code.left(6));
}

bool activateIfNeeded()
{
    QSettings settings;
    if (settings.value(QStringLiteral("activation/succeeded"), false).toBool())
        return true;

    while (true) {
        QInputDialog dialog;
        dialog.setWindowTitle(QStringLiteral("欢迎使用 VIncinzo 磁盘清理"));
        dialog.setLabelText(QStringLiteral("第一次见面，请输入作者给你的 10 位激活码哦。激活后以后就不用再输入啦。"));
        dialog.setTextEchoMode(QLineEdit::Normal);
        dialog.setOkButtonText(QStringLiteral("激活"));
        dialog.setCancelButtonText(QStringLiteral("暂不使用"));

        if (dialog.exec() != QDialog::Accepted)
            return false;

        const QString code = dialog.textValue().trimmed().toUpper();
        if (isValidActivationCode(code)) {
            settings.setValue(QStringLiteral("activation/succeeded"), true);
            settings.setValue(QStringLiteral("activation/code"), code);
            settings.sync();
            QMessageBox::information(nullptr, QStringLiteral("激活成功"),
                                     QStringLiteral("激活成功！你的专属清理助手已经准备好了，电脑整理这件事交给我吧。"));
            return true;
        }

        QMessageBox::warning(nullptr, QStringLiteral("激活码还不对哦"),
                             QStringLiteral("别着急，请核对作者提供的 10 位激活码（仅含大写字母和数字），再试一次就好。"));
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
