#pragma once
#include "ui_themes.h"

// Workspace default only: explicit workspace snapshots stay client-owned.
// The default is terminal-theme in config.conf unless Band saved a choice
// (a theme snapshot in the overrides file), which takes precedence.
class UiAppearance final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int themeIndex READ themeIndex NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool saved READ saved NOTIFY changed)
public:
    struct Candidate {QString name,path,error;QByteArray text;int builtin=0;QVariantMap snapshot;};
    UiAppearance(const QString &path,bool apple,UiThemes &,QObject *parent=nullptr,QString overrides={});
    int themeIndex() const {return index_;}
    QString error() const {return error_;}
    bool saved() const;
    QString overridesPath() const {return overrides_;}
    // Saves the theme as Band's default and uses it now.
    Q_INVOKABLE bool saveDefault(int index);
    // Removes Band's choice; terminal-theme in config.conf applies again.
    Q_INVOKABLE bool useConfigured();
    Candidate prepare() const;
    QString sourcePath() const;
    bool adopt(const Candidate &);
Q_SIGNALS:
    void changed();
private:
    QString source(QString &error) const;
    QVariantMap savedSnapshot(QString &error) const;
    QString path_,overrides_,error_;bool apple_;UiThemes &themes_;int index_=0;
};
