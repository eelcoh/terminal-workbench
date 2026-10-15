#include <QAccessible>
#include <QGuiApplication>
#include <QImage>
#include <QInputMethodEvent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>
#include <cstdio>

static bool check(bool ok, const char *message)
{
    if (!ok) qCritical() << "FAIL:" << message;
    return ok;
}

static bool selfTest(QQuickWindow *window)
{
    auto *field = window->findChild<QQuickItem *>("workspaceName");
    auto *button = window->findChild<QQuickItem *>("newTerminal");
    if (!check(field && button, "QML controls loaded")) return false;
    field->forceActiveFocus();
    bool ok = true;
    QInputMethodEvent preedit(QString::fromUtf8("にほん"), {});
    QGuiApplication::sendEvent(field, &preedit);
    ok &= check(field->property("text").toString().isEmpty(), "preedit must not commit");
    ok &= check(field->property("preeditText").toString() == QString::fromUtf8("にほん"), "preedit visible");
    QInputMethodEvent cancel;
    QGuiApplication::sendEvent(field, &cancel);
    ok &= check(field->property("text").toString().isEmpty() && field->property("preeditText").toString().isEmpty(), "cancel does not commit");
    const auto before = field->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    QInputMethodEvent commit;
    commit.setCommitString(QString::fromUtf8("日本"));
    QGuiApplication::sendEvent(field, &commit);
    ok &= check(field->property("text").toString() == QString::fromUtf8("日本"), "commit exactly once");
    const auto after = field->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    ok &= check(!after.isEmpty() && after.x() > before.x(), "input method caret follows committed text");
    QAccessible::setActive(true);
    auto *edit = QAccessible::queryAccessibleInterface(field);
    auto *action = QAccessible::queryAccessibleInterface(button);
    ok &= check(edit && edit->role() == QAccessible::EditableText && edit->text(QAccessible::Name) == "Workspace name", "editable text role and name");
    ok &= check(edit && edit->textInterface() && edit->textInterface()->text(0, -1) == QString::fromUtf8("日本"), "accessible text content");
    ok &= check(action && action->role() == QAccessible::Button && action->text(QAccessible::Name) == "New terminal", "button role and name");
    auto *actions = action ? action->actionInterface() : nullptr;
    ok &= check(actions && actions->actionNames().contains(QAccessibleActionInterface::pressAction()), "accessible press action advertised");
    if (actions) actions->doAction(QAccessibleActionInterface::pressAction());
    ok &= check(window->property("presses").toInt() == 1, "accessible press invokes button");
    if (ok) std::puts("PASS: composition/cancel/commit, caret geometry, accessible roles/text/action");
    return ok;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("Workbench Input Probe");
    app.setDesktopFileName("terminal-workbench-input-probe");
    QQuickStyle::setStyle("Basic");
    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) return 1;
    if (app.arguments().contains("--self-test")) {
        QTimer::singleShot(300, &app, [&] {
            bool ok = selfTest(window);
            const int index = app.arguments().indexOf("--screenshot");
            if (index >= 0 && index + 1 < app.arguments().size())
                ok &= check(window->grabWindow().save(app.arguments().at(index + 1)), "capture probe window");
            app.exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
