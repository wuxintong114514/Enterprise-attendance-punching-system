
#include "mainwindow.h"

#include <QApplication>
#include <QProcess>
#include <QMessageBox>
#include <QTimer>

// 定义最大重试次数
const int MAX_RETRIES = 3;

// 尝试连接数据库的函数
bool tryConnectToDatabase() {
    for (int i = 0; i < MAX_RETRIES; ++i) {
        if (MainWindow::createMySqlConn()) {
            return true;
        }
        // 如果连接失败，等待一段时间后重试
        if (i < MAX_RETRIES - 1) {
            QTimer::singleShot(2000, []() {}); // 等待2秒
        }
    }
    return false;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    if (!tryConnectToDatabase()) {
        // 尝试启动 MySQL 服务
        QProcess process;
        process.start("C:/Program Files/MySQL/MySQL Server 8.0/bin/mysqld.exe");
        if (!process.waitForStarted()) {
            QMessageBox::critical(nullptr, "错误", "无法启动 MySQL 服务");
            return 1;
        }

        // 再次尝试连接数据库
        if (!tryConnectToDatabase()) {
            QMessageBox::critical(nullptr, "错误", "无法连接到数据库，请检查 MySQL 服务和配置");
            return 1;
        }
    }

    MainWindow w;
    w.show();
    return a.exec();
}
