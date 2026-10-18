#pragma once
#include <QObject>
#include <QVariantMap>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <QUrl>
#include "terminal_item.h"
#include "runtime_facts.h"
#include "ui_lifecycle.h"
#include <QHash>

// Client adapter for the existing organization service. Never owns PTYs/catalog data.
class UiSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool savedSession READ savedSession CONSTANT)
    Q_PROPERTY(QVariantMap initialCatalog READ initialCatalog CONSTANT)
    Q_PROPERTY(QString defaultDirectory READ defaultDirectory CONSTANT)
    Q_PROPERTY(QVariantMap lifecycle READ lifecycle NOTIFY lifecycleChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit UiSession(const QJsonObject &config,QObject *parent=nullptr);
    bool savedSession() const {return config_["saved_session"].toBool();}
    QVariantMap initialCatalog() const {return config_["catalog"].toObject().toVariantMap();}
    QString defaultDirectory() const {return config_["cwd"].toString();}
    bool busy() const {return busy_;}
    QVariantMap lifecycle() const{return lifecycle_.values();}
    Q_INVOKABLE void inspect(const QString &identity);
    Q_INVOKABLE void reconnect(TerminalItem *item,const QString &identity,bool takeover=false);
    Q_INVOKABLE void recover(const QString &identity);
    Q_INVOKABLE void closeTerminal(const QString &identity);
    QString error() const {return error_;}
    Q_INVOKABLE QObject *observe(const QString &identity);
    Q_INVOKABLE void attach(TerminalItem *item,const QString &identity);
    Q_INVOKABLE void createTerminal(const QString &workspace);
    Q_INVOKABLE void createWorkspace(const QString &name,const QString &directory);
    Q_INVOKABLE void renameWorkspace(const QString &identity,const QString &name);
    Q_INVOKABLE void setWorkspaceDirectory(const QString &identity,const QString &directory);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE QUrl folderStart(const QString &path) const;
    Q_INVOKABLE QString folderPath(const QUrl &url) const;
    Q_INVOKABLE void moveTerminal(const QString &identity,const QString &workspace,int position);
    Q_INVOKABLE void refresh();
Q_SIGNALS:
    void changed();
    void lifecycleChanged();
    void actionCompleted(QString identity,QString method);
    void completed(QVariantMap catalog,QString method);
private:
    void request(const QString &method,const QString &identity={},const QString &name={},const QString &directory={},const QString &destination={},int position=-1);
    void fail(const QString &message);
    void prune(const QJsonObject &catalog);
    UiLifecycle lifecycle_;
    QHash<QString,RuntimeFacts *> facts_;
    QJsonObject config_;QLocalSocket socket_;QTimer deadline_;QByteArray receive_;QString method_,identity_,error_;bool busy_=false,sent_=false;
};
