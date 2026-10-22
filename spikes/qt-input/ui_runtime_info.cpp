#include "ui_runtime_info.h"
#include "local_socket.h"
#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>
#include <cmath>

namespace {
constexpr int limit=32768;
bool launchPath(const QJsonValue &value) {
    if(value.isUndefined() || value.isNull())return true;
    const auto text=value.toString();
    return value.isString() && text.toUtf8().size()<=4096 && !text.contains(QChar::Null) && QDir::isAbsolutePath(text);
}
QVariantMap unavailable() {return {{"state","unavailable"},{"message","Runtime details unavailable. This terminal may be stopped, disconnected or using an older runtime."}};}
}
UiRuntimeInfo::UiRuntimeInfo(QString directory,QString configuredRuntime,QObject *parent,int timeoutMs)
    :QObject(parent),directory_(std::move(directory)),configuredRuntime_(std::move(configuredRuntime)),timeoutMs_(timeoutMs){}
void UiRuntimeInfo::cancel() {
    if(!socket_)return;
    auto *previous=socket_;socket_=nullptr;previous->abort();previous->deleteLater();
}
void UiRuntimeInfo::clear() {cancel();report_.clear();Q_EMIT changed();}
void UiRuntimeInfo::finish(QLocalSocket *socket,const QVariantMap &value) {
    if(socket_!=socket)return;
    const auto identity=report_.value("identity");cancel();report_=value;report_.insert("identity",identity);Q_EMIT changed();
}
void UiRuntimeInfo::inspect(const QString &identity) {
    cancel();report_.clear();
    static const QRegularExpression valid("^[a-zA-Z0-9_-]{1,64}$");
    if(!valid.match(identity).hasMatch()){Q_EMIT changed();return;}
    report_={{"identity",identity},{"state","checking"},{"message","Checking this runtime…"}};Q_EMIT changed();
    const auto endpoint=QDir(directory_).filePath(identity+".ctl");
    if(!ownedLocalSocket(endpoint)){report_=unavailable();report_.insert("identity",identity);Q_EMIT changed();return;}
    auto *socket=new QLocalSocket(this);socket_=socket;
    socket->setReadBufferSize(limit+1);
    auto *timer=new QTimer(socket);timer->setSingleShot(true);timer->setInterval(timeoutMs_);
    connect(timer,&QTimer::timeout,this,[this,socket]{finish(socket,unavailable());});
    connect(socket,&QLocalSocket::connected,this,[this,socket]{if(socket_==socket)socket->write("CAPS\n");});
    connect(socket,&QLocalSocket::readyRead,this,[this,socket]{consume(socket);});
    connect(socket,&QLocalSocket::disconnected,this,[this,socket]{consume(socket);finish(socket,unavailable());});
    connect(socket,&QLocalSocket::errorOccurred,this,[this,socket](auto){finish(socket,unavailable());});
    timer->start();socket->connectToServer(endpoint);
}
void UiRuntimeInfo::consume(QLocalSocket *socket) {
    if(socket_!=socket)return;
    const auto bytes=socket->peek(limit+1);
    if(bytes.size()>limit){finish(socket,unavailable());return;}
    if(!bytes.contains('\n'))return;
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes.trimmed(),&error);const auto reply=document.object();
    if(error.error!=QJsonParseError::NoError || !document.isObject() || reply["protocol"].toDouble()!=1 || !reply["capabilities"].isArray()
        || reply["capabilities"].toArray().size()>64) {finish(socket,unavailable());return;}
    QStringList capabilities;
    for(const auto &entry:reply["capabilities"].toArray()) {
        const auto name=entry.toString();
        if(!entry.isString() || name.isEmpty() || name.size()>128 || name.contains(QRegularExpression("[\\x00-\\x1f\\x7f]"))){finish(socket,unavailable());return;}
        if(!capabilities.contains(name))capabilities.append(name);
    }
    const auto pid=reply["runtime_pid"];
    if(!pid.isUndefined() && (!pid.isDouble() || pid.toDouble()<1 || pid.toDouble()>2147483647 || std::floor(pid.toDouble())!=pid.toDouble())){finish(socket,unavailable());return;}
    const auto origin=reply["origin"].toObject();
    const bool hasOrigin=origin["version"].toDouble()==1 && launchPath(origin["runtime_binary"]) && launchPath(origin["agent_bin"]);
    QVariantMap value{{"state","available"},{"message","Capabilities from the last explicit runtime check."},{"capabilities",capabilities},
        {"origin_available",hasOrigin},{"runtime_binary",hasOrigin?origin["runtime_binary"].toString():QString()},
        {"agent_bin",hasOrigin?origin["agent_bin"].toString():QString()}};
    if(!pid.isUndefined())value.insert("runtime_pid",pid.toInteger());
    finish(socket,value);
}
QString UiRuntimeInfo::diagnostics() const {
    if(report_.isEmpty())return {};
    auto value=report_;value.insert("version",1);value.insert("configured_runtime",configuredRuntime_);
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(value)).toJson(QJsonDocument::Indented));
}
bool UiRuntimeInfo::copyDiagnostics() const {
    if(report_.isEmpty() || report_.value("state")=="checking")return false;
    QGuiApplication::clipboard()->setText(diagnostics());return true;
}
