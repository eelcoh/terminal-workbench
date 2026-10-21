#pragma once
#include <QObject>
#include <QVariantMap>
#include <QMap>
#include <QFileSystemWatcher>
#include <QTimer>

// Session-scoped client preferences. No session RPC, runtime or terminal access.
// Each stable workspace ID owns an independent bounded/atomic preference file.
class UiWorkspaceAppearance final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString directory READ directory CONSTANT)
public:
    explicit UiWorkspaceAppearance(const QString &directory,bool watch=true,QObject *parent=nullptr);
    QString directory() const {return directory_;}
    QString error() const;
    Q_INVOKABLE void setWorkspaces(const QStringList &ids);
    Q_INVOKABLE QVariantMap choice(const QString &id) const {return choices_.value(id);}
    Q_INVOKABLE bool begin(const QString &id);
    Q_INVOKABLE void end();
    Q_INVOKABLE bool apply(const QString &id,bool follow,const QVariantMap &theme);
    Q_INVOKABLE void reload();
    static bool validChoice(const QVariantMap &value);
Q_SIGNALS:
    void changed();
    void reloadFailed();
private:
    void watchSources();
    bool fail(const QString &message);
    QString directory_,editing_,editError_;QStringList ids_;
    QByteArray editSource_;
    QMap<QString,QVariantMap> choices_;QMap<QString,QString> errors_;
    QFileSystemWatcher watcher_;QTimer debounce_;bool watching_;
};
