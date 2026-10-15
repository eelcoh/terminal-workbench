#include "terminal_item.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>
#include <QInputMethodEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QImage>

int main(int argc,char **argv)
{
    QGuiApplication app(argc,argv);
    app.setApplicationName("Workbench Terminal Probe");app.setDesktopFileName("terminal-workbench-terminal-probe");
    QQuickStyle::setStyle("Basic");
    auto args=app.arguments();int control=args.indexOf("--control");
    if(control<0 || control+1>=args.size()) {std::fputs("usage: workbench-terminal-probe --control SESSION.ctl\n",stderr);return 2;}
    qmlRegisterType<TerminalItem>("Workbench.Probe",1,0,"TerminalItem");
    QQmlApplicationEngine engine;engine.load(QUrl("qrc:/Terminal.qml"));
    if(engine.rootObjects().isEmpty())return 1;
    auto *terminal=engine.rootObjects().first()->findChild<TerminalItem *>("terminal");
    if(!terminal)return 1;
    engine.rootObjects().first()->setProperty("disposable",args.contains("--disposable"));
    terminal->connectSession(args.at(control+1));
    // Test reports contain only the disposable test session, and are opt-in.
    int reportIndex=args.indexOf("--test-report");
    QTimer reportTimer;
    int imeStage=0;
    bool imeDone=false;
    if(reportIndex>=0 && reportIndex+1<args.size()) {
        const QString report=args.at(reportIndex+1);
        auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        reportTimer.setInterval(80);
        QObject::connect(&reportTimer,&QTimer::timeout,&app,[&,report,window]{
            if(terminal->ready() && args.contains("--test-ime") && imeStage<3
                && terminal->viewportText().contains("QT-SNAPSHOT")) {
                terminal->forceActiveFocus();
                QInputMethodEvent event(imeStage==0?QString::fromUtf8("にほん"):QString(),{});
                if(imeStage==2)event.setCommitString(QString::fromUtf8("日本"));
                QGuiApplication::sendEvent(terminal,&event);
                imeDone=++imeStage==3;
            }
            const auto caret=terminal->caret();const auto cell=terminal->cellSize();
            const auto pos=terminal->mapToScene(QPointF());
            QJsonObject data{{"ready",terminal->ready()},{"status",terminal->status()},
                {"text",terminal->viewportText()},{"ime_done",imeDone},
                {"cell_width",cell.width()},{"cell_height",cell.height()},
                {"x",pos.x()},{"y",pos.y()},{"width",terminal->width()},{"height",terminal->height()},
                {"caret",QJsonArray{caret.x(),caret.y(),caret.width(),caret.height()}}};
            QSaveFile file(report);
            if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(data).toJson());file.commit();}
            static bool captured=false;
            if(terminal->ready() && !captured && terminal->viewportText().contains("QT-SNAPSHOT")) {
                captured=window->grabWindow().save(report+".png");
            }
        });
        reportTimer.start();
    }
    return app.exec();
}
