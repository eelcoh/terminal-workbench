// Isolated UI slice; the launcher owns disposable runtime lifecycle.
#include "terminal_item.h"
#include "terminal_accessibility.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

// Repeater delegates have a visual parent without necessarily being QObject children.
static TerminalItem *findTerminal(QQuickItem *root,const QString &name) {
    if(root->objectName()==name)return qobject_cast<TerminalItem *>(root);
    for(auto *child:root->childItems())if(auto *item=findTerminal(child,name))return item;
    return nullptr;
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    app.setApplicationName("Terminal Workbench UI Study");
    app.setDesktopFileName("terminal-workbench-ui-study");
    QQuickStyle::setStyle("Basic");
    if(argc<2)return 2;
    QFile config(QString::fromLocal8Bit(argv[1]));if(!config.open(QIODevice::ReadOnly))return 2;
    const auto paths=QJsonDocument::fromJson(config.readAll()).array();if(paths.size()!=6)return 2;
    qmlRegisterType<TerminalItem>("Workbench.Probe",1,0,"TerminalItem");installTerminalAccessibility();
    QQmlApplicationEngine engine;engine.load(QUrl("qrc:/Ui.qml"));if(engine.rootObjects().isEmpty())return 1;
    auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());if(!window)return 1;
    QVector<TerminalItem *> terminals;
    for(int i=0;i<paths.size();++i) {
        auto *item=findTerminal(window->contentItem(),"terminal"+QString::number(i));if(!item)return 1;
        item->connectSession(paths[i].toString());terminals.append(item);
    }
    // Explicit test-only snapshots; no user terminal content is recorded by default.
    const auto args=app.arguments();const int reportIndex=args.indexOf("--test-report");QTimer reporter;
    if(reportIndex>=0 && reportIndex+1<args.size()) {
        const auto path=args[reportIndex+1];reporter.setInterval(80);
        QObject::connect(&reporter,&QTimer::timeout,&app,[&,path] {
            QJsonArray panes;
            for(auto *item:terminals) {
                const auto size=item->terminalGridSize();const auto position=item->mapToScene(QPointF(item->width()/2,item->height()/2));
                panes.append(QJsonObject{{"ready",item->ready()},{"text",item->viewportText()},
                    {"columns",size.width()},{"rows",size.height()},{"focus",item->hasActiveFocus()},
                    {"x",position.x()},{"y",position.y()},{"rasterized_rows",double(item->rasterizedRows())}});
            }
            const QJsonObject data{{"workspace",window->property("workspace").toInt()},
                {"selected",window->property("selected").toInt()},{"overview",window->property("overview").toBool()},
                {"content_x",window->findChild<QQuickItem *>("deck")->property("contentX").toDouble()},
                {"panes",panes},{"platform",QGuiApplication::platformName()}};
            QSaveFile report(path);if(report.open(QIODevice::WriteOnly)){report.write(QJsonDocument(data).toJson());report.commit();}
            if(QFile::exists(path+".capture") && window->grabWindow().save(path+".png"))QFile::remove(path+".capture");
        });reporter.start();
    }
    return app.exec();
}
