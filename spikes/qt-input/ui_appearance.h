#pragma once
#include "ui_themes.h"

// Workspace default only: explicit workspace snapshots stay client-owned.
class UiAppearance final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int themeIndex READ themeIndex NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    struct Candidate {QString name,path,error;QByteArray text;int builtin=0;};
    UiAppearance(const QString &path,bool apple,UiThemes &,QObject *parent=nullptr);
    int themeIndex() const {return index_;}
    QString error() const {return error_;}
    Candidate prepare() const;
    QString sourcePath() const;
    bool adopt(const Candidate &);
Q_SIGNALS:
    void changed();
private:
    QString source(QString &error) const;
    QString path_,error_;bool apple_;UiThemes &themes_;int index_=0;
};
