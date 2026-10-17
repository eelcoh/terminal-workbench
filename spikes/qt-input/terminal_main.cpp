#include "terminal_item.h"
#include "terminal_accessibility.h"
#include "platform_keys.h"
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
#include <QFile>

static QJsonObject terminalReport(TerminalItem *terminal)
{
    const auto caret=terminal->caret();const auto cell=terminal->cellSize();
    const auto ime=terminal->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    const auto pos=terminal->mapToScene(QPointF());
    return {{"ready",terminal->ready()},{"status",terminal->status()},
        {"text",terminal->viewportText()},{"preedit",terminal->preeditText()},
        {"terminal_focus",terminal->hasActiveFocus()},
        {"ime_caret",QJsonArray{ime.x(),ime.y(),ime.width(),ime.height()}},
        {"text_raster_scale",terminal->textRasterScale()},
        {"cell_width",cell.width()},{"cell_height",cell.height()},
        {"x",pos.x()},{"y",pos.y()},{"width",terminal->width()},{"height",terminal->height()},
        {"caret",QJsonArray{caret.x(),caret.y(),caret.width(),caret.height()}}};
}

int main(int argc,char **argv)
{
    QGuiApplication app(argc,argv);
    app.setApplicationName("Workbench Terminal Probe");app.setDesktopFileName("terminal-workbench-terminal-probe");
    QQuickStyle::setStyle("Basic");
    auto args=app.arguments();int control=args.indexOf("--control");
    if(control<0 || control+1>=args.size()) {std::fputs("usage: workbench-terminal-probe --control SESSION.ctl\n",stderr);return 2;}
    const int second=args.indexOf("--second-control");
    if(second>=0 && second+1>=args.size()) {std::fputs("--second-control requires a path\n",stderr);return 2;}
    qmlRegisterType<TerminalItem>("Workbench.Probe",1,0,"TerminalItem");
    QQmlApplicationEngine engine;installTerminalAccessibility();engine.load(QUrl(second>=0?"qrc:/Pair.qml":"qrc:/Terminal.qml"));
    if(engine.rootObjects().isEmpty())return 1;
    auto *terminal=engine.rootObjects().first()->findChild<TerminalItem *>("terminal");
    if(!terminal)return 1;
    engine.rootObjects().first()->setProperty("disposable",args.contains("--disposable"));
    terminal->connectSession(args.at(control+1));
    auto *peer=engine.rootObjects().first()->findChild<TerminalItem *>("peer");
    if(second>=0) {
        if(!peer)return 1;
        peer->connectSession(args.at(second+1));
    }
    // Test reports contain only the disposable test session, and are opt-in.
    int reportIndex=args.indexOf("--test-report");
    QTimer reportTimer;
    int imeStage=0;bool keysDone=false;
    bool imeDone=false, accessibilityDone=false, accessibilityOk=false;
    if(reportIndex>=0 && reportIndex+1<args.size()) {
        const QString report=args.at(reportIndex+1);
        auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        auto *copyButton=window->findChild<QQuickItem *>("copyButton");
        if(!copyButton)return 1;
        auto *pasteButton=window->findChild<QQuickItem *>("pasteButton");
        reportTimer.setInterval(80);
        QObject::connect(&reportTimer,&QTimer::timeout,&app,[&,report,window,copyButton,pasteButton]{
            if(terminal->ready() && args.contains("--test-ime") && imeStage<3
                && terminal->viewportText().contains("QT-SNAPSHOT")) {
                if(imeStage==0 && !terminalCompositionSelfTest()) {app.exit(1);return;}
                terminal->forceActiveFocus();
                QInputMethodEvent event(imeStage==0?QString::fromUtf8("にほん"):QString(),{});
                if(imeStage==2)event.setCommitString(QString::fromUtf8("日本"));
                QGuiApplication::sendEvent(terminal,&event);
                imeDone=++imeStage==3;
            }
            if(args.contains("--test-accessibility") && !accessibilityDone && terminal->ready()
                && terminal->viewportText().contains(QString::fromUtf8("漢"))) {
                accessibilityOk=terminalAccessibilitySelfTest(terminal);accessibilityDone=true;
                if(!accessibilityOk) {app.exit(1);return;}
            }
            if(args.contains("--test-keys") && !keysDone && terminal->ready()
                && terminal->viewportText().contains("QT-KEY-PROBE")) {
                terminal->forceActiveFocus();
                const auto key=[&](int code,Qt::KeyboardModifiers mods,bool repeat=false) {
                    QKeyEvent press(QEvent::KeyPress,code,mods,QString(),repeat);
                    QKeyEvent release(QEvent::KeyRelease,code,mods);
                    QGuiApplication::sendEvent(terminal,&press);QGuiApplication::sendEvent(terminal,&release);
                };
                key(Qt::Key_Left,Qt::NoModifier);
                key(Qt::Key_Left,Qt::AltModifier);
                key(Qt::Key_Right,Qt::AltModifier);key(Qt::Key_Right,Qt::AltModifier,true);
                key(Qt::Key_Right,Qt::NoModifier);
#ifdef Q_OS_MACOS
                key(Qt::Key_Left,Qt::MetaModifier); // physical Control on Cocoa
#else
                key(Qt::Key_Left,Qt::ControlModifier);
#endif
                // Exercise the shared policy, including adjacent-modifier exclusions.
                if(QByteArray(wordMotion(Qt::Key_Left,Qt::AltModifier))!=QByteArray("\x1b" "b")
                    || QByteArray(wordMotion(Qt::Key_Right,Qt::AltModifier))!=QByteArray("\x1b" "f")
                    || wordMotion(Qt::Key_Left,Qt::AltModifier|Qt::ShiftModifier)
                    || wordMotion(Qt::Key_Left,Qt::AltModifier|Qt::MetaModifier)
                    || wordMotion(Qt::Key_Left,Qt::AltModifier|Qt::ControlModifier)
                    || wordMotion(Qt::Key_Up,Qt::AltModifier)) {app.exit(1);return;}
                keysDone=true;
            }
            const auto copyCenter=copyButton->mapToScene(QPointF(copyButton->width()/2,copyButton->height()/2));
            QJsonObject data=terminalReport(terminal);
            data.insert("keys_done",keysDone);data.insert("ime_done",imeDone);
            data.insert("copy_button",QJsonArray{copyCenter.x(),copyCenter.y()});
            data.insert("accessibility_ok",accessibilityDone && accessibilityOk);
            data.insert("platform",QGuiApplication::platformName());
            data.insert("device_pixel_ratio",window->devicePixelRatio());
            data.insert("window_width",window->width());data.insert("window_height",window->height());
            if(peer)data.insert("peer",terminalReport(peer));
            if(pasteButton) {
                const auto center=pasteButton->mapToScene(QPointF(pasteButton->width()/2,pasteButton->height()/2));
                data.insert("paste_button",QJsonArray{center.x(),center.y()});
            }
            QSaveFile file(report);
            if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(data).toJson());file.commit();}
            static bool captured=false;
            const QString request=report+".capture";
            if(terminal->ready() && ((!captured && terminal->viewportText().contains("QT-SNAPSHOT")) || QFile::exists(request))) {
                captured=window->grabWindow().save(report+".png");
                if(captured)QFile::remove(request);
            }
        });
        reportTimer.start();
    }
    return app.exec();
}
