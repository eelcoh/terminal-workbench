#include "deployment_check.h"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QQuickStyle>
#include <cstdio>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 2) return 2;
    QQuickStyle::setStyle("Basic");
    const auto report = deploymentCheck(QString::fromLocal8Bit(argv[1]));
    if (!report["qml_ok"].toBool()) {
        std::fprintf(stderr, "%s\n", QJsonDocument(report).toJson().constData());
        return 1;
    }
    std::puts("PASS: copied QML closure instantiates Basic controls, layouts and dialog components without installation import fallback");
    return 0;
}
