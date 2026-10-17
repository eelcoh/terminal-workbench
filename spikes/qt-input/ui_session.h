#pragma once
#include <QObject>
#include <QVariantMap>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include "terminal_item.h"

// Client adapter for the existing organization service. Never owns PTYs/catalog data.
class UiSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList themes READ themes CONSTANT)
    Q_PROPERTY(QVariantMap initialCatalog READ initialCatalog CONSTANT)
    Q_PROPERTY(QString defaultDirectory READ defaultDirectory CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit UiSession(const QJsonObject &config,QObject *parent=nullptr);
    QVariantList themes() const {return themes_;}
    QVariantMap initialCatalog() const {return config_["catalog"].toObject().toVariantMap();}
    QString defaultDirectory() const {return config_["cwd"].toString();}
    bool busy() const {return busy_;}
    QString error() const {return error_;}
    Q_INVOKABLE void attach(TerminalItem *item,const QString &identity);
    Q_INVOKABLE void createTerminal(const QString &workspace);
    Q_INVOKABLE void createWorkspace(const QString &name,const QString &directory);
    Q_INVOKABLE void refresh();
Q_SIGNALS:
    void changed();
    void completed(QVariantMap catalog,QString method);
private:
    void request(const QString &method,const QString &identity={},const QString &name={},const QString &directory={});
    void fail(const QString &message);
    QVariantList themes_;
    QJsonObject config_;QLocalSocket socket_;QTimer deadline_;QByteArray receive_;QString method_,error_;bool busy_=false,sent_=false;
};
