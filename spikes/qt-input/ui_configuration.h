#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMap>
#include "ui_fonts.h"
#include "ui_appearance.h"
#include "ui_shortcuts.h"

// Client-only, event-driven settings refresh. No session/runtime access.
class UiConfiguration final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int themeIndex READ themeIndex NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString managedPath READ managedPath CONSTANT)
    Q_PROPERTY(QString fontPath READ fontPath CONSTANT)
    Q_PROPERTY(QString shortcutPath READ shortcutPath CONSTANT)
    Q_PROPERTY(bool editing READ editing WRITE setEditing NOTIFY changed)
public:
    UiConfiguration(UiFonts &,UiShortcuts &,bool watch=true,QObject *parent=nullptr,UiAppearance *appearance=nullptr);
    int themeIndex() const {return appearance_?appearance_->themeIndex():0;}
    QString error() const {return error_;}
    QString status() const {return status_;}
    QString managedPath() const {return fonts_.managedPath();}
    QString fontPath() const {return fonts_.overridesPath();}
    QString shortcutPath() const {return shortcuts_.overridesPath();}
    bool editing() const {return editing_;}
    void setEditing(bool);
    Q_INVOKABLE bool reload();
    Q_INVOKABLE void openDirectory();
Q_SIGNALS:
    void changed();
    void reloadFailed();
private:
    QStringList sources() const;
    QMap<QString,QByteArray> snapshot() const;
    void watchSources();
    bool refresh(bool manual);
    UiAppearance *appearance_;
    UiFonts &fonts_;UiShortcuts &shortcuts_;
    QFileSystemWatcher watcher_;QTimer debounce_;
    QMap<QString,QByteArray> attempted_;
    bool watching_,editing_=false,pending_=false;
    QString error_,status_="Configuration loaded at startup";
};
