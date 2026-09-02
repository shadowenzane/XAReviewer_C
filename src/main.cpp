#include <QApplication>

#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("XAReviewer"));
    QApplication::setOrganizationName(QStringLiteral("XAReviewer"));
    QApplication::setApplicationVersion(QStringLiteral("1.0 (C++)"));

    // 命令行传入目录则直接加载
    MainWindow w;
    w.show();
    if (argc > 1)
        QMetaObject::invokeMethod(&w, "loadDirectory", Qt::QueuedConnection,
                                  Q_ARG(QString, QString::fromLocal8Bit(argv[1])));

    return app.exec();
}
