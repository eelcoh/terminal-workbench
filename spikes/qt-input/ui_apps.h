#pragma once
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QVariantList>

// The Apps list: a curated set of TUIs, the shipped catalog generated from
// awesome-tuis and Homebrew (scripts/prepare-app-catalog.py) and [app.ID]
// definitions, whether each command is found on the login shell's PATH, and
// the ways to install a missing one here. Probing runs two short processes;
// it never installs.
class UiApps final : public QObject {
    Q_OBJECT
    // Separate notifications: a list is re-read (and its rows rebuilt) only
    // when it changed, not on every check.
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(QString note READ note NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantMap homebrew READ homebrew NOTIFY sourcesChanged)
public:
    struct Catalog {QString id,name,command,description,section,brew,dnf,apt,pacman,copr;bool featured=false;};
    // Curated apps first (with system package names), then the shipped file's
    // apps for this platform that are not curated.
    static QList<Catalog> catalog(bool apple,const QString &file);
    // Where Homebrew installs itself. Found there but not on PATH, it is
    // installed but the shell does not load it yet.
    static QStringList brewLocations();
    explicit UiApps(bool apple,QString shell,QObject *parent=nullptr,QString imageMarker=QStringLiteral("/run/ostree-booted"),
        QString catalogFile=QStringLiteral(":/apps-catalog.json"),QString sourcesFile={},QStringList brewAt=brewLocations());
    QVariantList entries() const {return entries_;}
    bool checking() const {return checking_;}
    QString note() const {return note_;}
    QVariantList sources() const {return sources_;}
    QVariantMap homebrew() const {return homebrew_;}
    Q_INVOKABLE void setDefinitions(const QVariantList &definitions);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap entry(const QString &id) const;
    // A source is a package manager or extra repositories; one turned off is
    // not offered. Saved in the sources file.
    Q_INVOKABLE void setSourceEnabled(const QString &id,bool enabled);
    // The UI test replaces Homebrew's installer with a local script.
    void setHomebrewInstaller(const QString &command) {installer_=command;rebuild();}
    // The program a command starts, or empty when it cannot be checked.
    static QString program(const QString &command);
    static QString probeScript(const QStringList &programs,const QString &imageMarker,const QStringList &brewAt={});
Q_SIGNALS:
    void entriesChanged();
    void checkingChanged();
    void sourcesChanged();
    void checked();
private:
    void rebuild();
    void probe(const QByteArray &path);
    void finish(const QByteArray &output);
    bool enabled(const QString &source) const;
    bool apple_;QString shell_,imageMarker_,sourcesFile_;QStringList brewAt_;QList<Catalog> known_;
    QVariantList definitions_,entries_,sources_;QString note_,installer_;QVariantMap homebrew_;
    QHash<QString,int> installed_;QSet<QString> managers_;QHash<QString,bool> choices_;QString brewOffPath_;
    bool image_=false,checked_=false,checking_=false,again_=false;
    QProcess *process_=nullptr;QTimer deadline_;QStringList programs_;
};
