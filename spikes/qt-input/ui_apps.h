#pragma once
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QVariantList>

// The Apps list: a curated set of TUIs, the shipped catalog generated from
// awesome-tuis and Homebrew (scripts/prepare-app-catalog.py) and [app.ID]
// definitions,
// whether each command is found on the login shell's PATH, and how to install
// a missing one here. Probing runs two short processes; it never installs.
class UiApps final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(QString note READ note NOTIFY changed)
public:
    struct Catalog {QString id,name,command,description,section,brew,dnf,apt,pacman;bool featured=false;};
    // Curated apps first (with system package names), then the shipped file's
    // apps for this platform that are not curated.
    static QList<Catalog> catalog(bool apple,const QString &file);
    explicit UiApps(bool apple,QString shell,QObject *parent=nullptr,QString imageMarker=QStringLiteral("/run/ostree-booted"),QString catalogFile=QStringLiteral(":/apps-catalog.json"));
    QVariantList entries() const {return entries_;}
    bool checking() const {return checking_;}
    QString note() const {return note_;}
    Q_INVOKABLE void setDefinitions(const QVariantList &definitions);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap entry(const QString &id) const;
    // The program a command starts, or empty when it cannot be checked.
    static QString program(const QString &command);
    static QString probeScript(const QStringList &programs,const QString &imageMarker);
Q_SIGNALS:
    void changed();
    void checked();
private:
    void rebuild();
    void probe(const QByteArray &path);
    void finish(const QByteArray &output);
    bool apple_;QString shell_,imageMarker_;QList<Catalog> known_;
    QVariantList definitions_,entries_;QString note_;
    QHash<QString,int> installed_;QSet<QString> managers_;bool image_=false,checked_=false,checking_=false,again_=false;
    QProcess *process_=nullptr;QTimer deadline_;QStringList programs_;
};
