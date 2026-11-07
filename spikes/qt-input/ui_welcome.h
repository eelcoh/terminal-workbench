#pragma once
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QSaveFile>

// The welcome is due until its marker exists. Only launchers with saved
// sessions pass a marker; study and test windows never show it unasked.
class UiWelcome final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool due READ due NOTIFY changed)
public:
    explicit UiWelcome(QString marker,QObject *parent=nullptr)
        :QObject(parent),marker_(std::move(marker)),due_(!marker_.isEmpty() && !QFileInfo(marker_).exists() && !QFileInfo(marker_).isSymLink()) {}
    bool due() const {return due_;}
    Q_INVOKABLE void finish() {
        if(!due_)return;
        due_=false;Q_EMIT changed();
        QDir().mkpath(QFileInfo(marker_).absolutePath());
        QSaveFile file(marker_);
        if(file.open(QIODevice::WriteOnly)){file.write("1\n");file.commit();}
    }
Q_SIGNALS:
    void changed();
private:
    QString marker_;bool due_;
};
