#pragma once
#include "theme_store.h"
#include "theme_bridge.h"
#include <QObject>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

// Client appearance catalog. Separate from organization service/session ownership.
class UiThemes final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY catalogChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString storePath READ storePath CONSTANT)
public:
    explicit UiThemes(const QString &store,QObject *parent=nullptr,QNetworkAccessManager *network=nullptr,int timeoutMs=15000);
    QVariantList catalog() const {return catalog_;}
    bool busy() const {return busy_;}
    QString error() const {return error_;}
    QString status() const {return status_;}
    QString storePath() const {return directory_;}
    Q_INVOKABLE QVariantMap theme(int index) const;
    Q_INVOKABLE void importTheme(const QString &source,const QString &name={},const QString &attribution={});
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void clearError();
    static QUrl downloadUrl(const QString &source);
Q_SIGNALS:
    void changed();
    void catalogChanged();
    void imported(int index);
private:
    void append(int index,const StoredTheme *record=nullptr,int ignored=0);
    void collect();
    void finish(const QByteArray &text,const QString &source,const QString &name,const QString &attribution);
    void fail(const QString &error);
    static bool parse(const QByteArray &text,WorkbenchTheme &theme,int &ignored,QString &error);
    QString directory_,error_,storeError_,status_,pendingSource_,pendingName_,pendingAttribution_;
    QVector<StoredTheme> records_;
    QVariantList catalog_;QHash<int,QVariantMap> lookup_;QVector<int> importIndices_;
    QNetworkAccessManager *network_;QPointer<QNetworkReply> reply_;QByteArray download_;QTimer deadline_;bool busy_=false;
};
