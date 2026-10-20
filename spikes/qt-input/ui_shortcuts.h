#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>
#include <memory>
#include "shortcut_bridge.h"
class UiShortcuts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap sequences READ sequences NOTIFY changed)
    Q_PROPERTY(QVariantMap labels READ labels NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString editError READ editError NOTIFY changed)
    Q_PROPERTY(QVariantMap draftLabels READ draftLabels NOTIFY changed)
public:
    explicit UiShortcuts(const QString &path,bool apple,QObject *parent=nullptr,const QString &overrides={});
    std::unique_ptr<UiShortcuts> reloadCandidate() const;
    void adoptReload(const UiShortcuts &candidate);
    QString managedPath() const {return path_;}
    QString overridesPath() const {return overrides_;}
    QVariantMap sequences() const{return sequences_;}
    QVariantMap labels() const{return labels_;}
    QVariantMap draftLabels() const;
    QString error() const{return error_;}
    QString editError() const{return editError_;}
    Q_INVOKABLE bool editable(const QString &id) const;
    Q_INVOKABLE void begin();
    Q_INVOKABLE bool record(const QString &id,int key,int modifiers);
    Q_INVOKABLE bool clear(const QString &id);
    Q_INVOKABLE bool reset(const QString &id);
    Q_INVOKABLE bool apply();
Q_SIGNALS:
    void changed();
private:
    void rebuild();
    bool validate(const QVector<WorkbenchShortcut> &,QString &) const;
    bool set(const QString &,int,unsigned);
    int index(const QString &) const;
    QVariantMap sequences_,labels_;QString error_,editError_,path_,overrides_;
    bool apple_;QVector<WorkbenchShortcut> base_,current_,draft_;
    QByteArray managedBytes_,overrideBytes_;
};
