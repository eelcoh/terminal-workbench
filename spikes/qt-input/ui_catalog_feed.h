#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QLocalSocket>
#include <QTimer>
#include <QVariantMap>
#include <QJsonObject>
// Filesystem notifications invalidate a cached, server-read catalog. No PTY access.
class UiCatalogFeed final : public QObject {
    Q_OBJECT
public:
    UiCatalogFeed(const QString &directory,const QVariantMap &initial,QObject *parent=nullptr,int timeoutMs=2000,int settleMs=100);
    ~UiCatalogFeed() override;
    QString error() const {return error_;}
    void pause();
    void resume(const QVariantMap &confirmed={});
    void refresh();
    static bool valid(const QJsonObject &catalog);
Q_SIGNALS:
    void updated(QVariantMap catalog);
    void changed();
private:
    void watch();
    void schedule();
    void read();
    void fail(const QString &message,bool unsupported=false);
    void notice(const QString &message);
    QString directory_,path_,endpoint_,error_;QVariantMap known_;
    QFileSystemWatcher watcher_;QLocalSocket socket_;QTimer settle_,deadline_;
    QByteArray buffer_;bool paused_=false,reading_=false,dirty_=false,unsupported_=false;
};
