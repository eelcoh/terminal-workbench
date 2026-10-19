#pragma once
#include <QObject>
#include <QTimer>
#include <QSet>
class QQuickWindow;
// Opt-in, client-only metadata. No terminal snapshots, runtime paths or process data.
class UiDisplayReport:public QObject {
    Q_OBJECT
public:
    UiDisplayReport(QQuickWindow *,QString path);
protected:
    bool eventFilter(QObject *,QEvent *) override;
private Q_SLOTS:
    void schedule();
private:
    void write();
    QQuickWindow *window_;QString path_;QTimer timer_;QSet<QObject *> watched_;
};
