#include "ui_lifecycle.h"
#include <QTimer>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
void UiLifecycle::inspect(const QString &identity) {
    static const QRegularExpression id("^[a-zA-Z0-9_-]{1,64}$");
    if(!id.match(identity).hasMatch() || inspections_.contains(identity))return;
    auto *socket=new QLocalSocket(this);auto *deadline=new QTimer(socket);
    socket->setReadBufferSize(8193);deadline->setSingleShot(true);deadline->setInterval(timeoutMs_);
    inspections_.insert(identity,socket);
    auto checking=lifecycle_.value(identity).toMap();
    if(!checking.contains("message"))checking={{"state","unavailable"},{"message","Checking terminal status…"}};
    checking.insert("busy",true);lifecycle_.insert(identity,checking);Q_EMIT changed();
    const auto finish=[this,socket,identity](QVariantMap value){
        if(inspections_.value(identity)!=socket)return;
        inspections_.remove(identity);socket->abort();socket->deleteLater();
        value.insert("busy",false);lifecycle_.insert(identity,value);Q_EMIT changed();
    };
    const auto unavailable=[finish]{finish({{"state","unavailable"},{"message","Status unavailable. Check again."}});};
    connect(socket,&QLocalSocket::connected,this,[socket,identity]{socket->write(QJsonDocument(QJsonObject{{"protocol",1},{"method","terminal.lifecycle"},{"identity",identity}}).toJson(QJsonDocument::Compact)+'\n');});
    connect(socket,&QLocalSocket::readyRead,this,[socket,identity,finish,unavailable]{
        const auto bytes=socket->peek(8193);
        if(bytes.size()>8192) {unavailable();return;}
        if(!bytes.contains('\n'))return;
        QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes.trimmed(),&error);const auto reply=document.object();
        const auto state=reply["state"].toString();const auto detail=reply["lifecycle"].toObject();
        if(error.error!=QJsonParseError::NoError || reply["protocol"].toInt()!=1 || !reply["ok"].toBool() || reply["id"].toString()!=identity
            || (state!="running" && state!="stopped" && state!="unavailable") || !detail["message"].isString()) {unavailable();return;}
        finish({{"state",state},{"reason",detail["reason"].toString()},{"message",detail["message"].toString()}});
    });
    connect(socket,&QLocalSocket::disconnected,this,unavailable);
    connect(socket,&QLocalSocket::errorOccurred,this,[unavailable](auto){unavailable();});
    connect(deadline,&QTimer::timeout,this,unavailable);deadline->start();
    socket->connectToServer(endpoint_);
}

void UiLifecycle::retain(const QSet<QString> &active) {
    for(const auto &id:inspections_.keys())if(!active.contains(id)) {auto *socket=inspections_.take(id);socket->abort();socket->deleteLater();}
    bool changedValue=false;
    for(const auto &id:lifecycle_.keys())if(!active.contains(id)) {lifecycle_.remove(id);changedValue=true;}
    if(changedValue)Q_EMIT changed();
}
