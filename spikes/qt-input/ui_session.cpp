#include "ui_session.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>

UiSession::UiSession(const QJsonObject &config,QObject *parent):QObject(parent),lifecycle_(QDir(config["state"].toString()).filePath("service.rpc"),this),config_(config) {
    connect(&lifecycle_,&UiLifecycle::changed,this,&UiSession::lifecycleChanged);
    socket_.setReadBufferSize(256*1024+1);deadline_.setSingleShot(true);deadline_.setInterval(65000);
    connect(&deadline_,&QTimer::timeout,this,[this]{fail("Session service timed out. Refresh before trying again.");});
    connect(&socket_,&QLocalSocket::connected,this,[this] {
        if(!busy_)return;
        // No retry after sending a mutation: a lost reply does not prove failure.
        sent_=true;
        if(socket_.write(receive_)!=receive_.size()) {fail("Could not send session request. Refresh before trying again.");return;}
        receive_.clear();
    });
    connect(&socket_,&QLocalSocket::readyRead,this,[this] {
        if(!busy_)return;
        receive_+=socket_.readAll();
        if(receive_.size()>256*1024) {fail("Session service reply exceeds the size limit.");return;}
        const auto newline=receive_.indexOf('\n');if(newline<0)return;
        QJsonParseError parse;const auto document=QJsonDocument::fromJson(receive_.left(newline),&parse);
        if(parse.error!=QJsonParseError::NoError || !document.isObject() || newline!=receive_.size()-1) {fail("Invalid session service reply. Refresh before trying again.");return;}
        const auto reply=document.object();
        if(reply["protocol"].toInt()!=1) {fail("Incompatible session service.");return;}
        if(!reply["ok"].toBool()) {fail(reply["error"].toString("Session request failed. Refresh before trying again."));return;}
        if(!reply["data"].isObject() || !UiCatalogFeed::valid(reply["data"].toObject())) {fail("Missing session catalog. Refresh before trying again.");return;}
        const auto catalog=reply["data"].toObject().toVariantMap();const auto method=method_,identity=identity_;
        deadline_.stop();busy_=false;socket_.abort();receive_.clear();if(catalog_)catalog_->resume(catalog);Q_EMIT changed();Q_EMIT completed(catalog,method);prune(reply["data"].toObject());Q_EMIT actionCompleted(identity,method);
    });
    connect(&socket_,&QLocalSocket::disconnected,this,[this]{if(busy_)fail("Session service closed before replying. Refresh before trying again.");});
    connect(&socket_,&QLocalSocket::errorOccurred,this,[this](auto){if(busy_)fail(sent_?"Request outcome is unknown. Refresh before trying again.":socket_.errorString());});
    if(config["catalog_updates"].toBool(config["saved_session"].toBool())){
        catalog_=new UiCatalogFeed(config["state"].toString(),initialCatalog(),this);
        connect(catalog_,&UiCatalogFeed::changed,this,&UiSession::catalogNoticeChanged);
        connect(catalog_,&UiCatalogFeed::updated,this,[this](const QVariantMap &data){Q_EMIT completed(data,"catalog.update");prune(QJsonObject::fromVariantMap(data));});
    }

}
void UiSession::fail(const QString &message) {
    busy_=false;deadline_.stop();socket_.abort();receive_.clear();if(catalog_)catalog_->resume();error_=message;Q_EMIT changed();
}
QObject *UiSession::observe(const QString &identity) {
    static const QRegularExpression id("^[a-zA-Z0-9_-]{1,64}$");
    if(!id.match(identity).hasMatch())return nullptr;
    if(!facts_.contains(identity))facts_.insert(identity,new RuntimeFacts(QDir(config_["state"].toString()).filePath(identity+".ctl"),this));
    return facts_.value(identity);
}
void UiSession::attach(TerminalItem *item,const QString &identity) {
    static const QRegularExpression id("^[a-zA-Z0-9_-]{1,64}$");
    if(item && id.match(identity).hasMatch())item->connectSession(QDir(config_["state"].toString()).filePath(identity+".ctl"));
}
void UiSession::request(const QString &method,const QString &identity,const QString &name,const QString &directory,const QString &destination,int position) {
    if(busy_)return;
    if(catalog_)catalog_->pause();
    socket_.abort();sent_=false;busy_=true;error_.clear();method_=method;identity_=identity;
    QJsonObject message{{"protocol",1},{"method",method},{"identity",identity},{"title",name},
        {"runtime",config_["runtime"]},{"cwd",config_["cwd"]}};
    if(method=="terminal.move") {message.insert("destination",destination);message.insert("position",position);}
    if(method=="workspace.create_at" || method=="workspace.directory")message.insert("project_directory",directory);
    receive_=QJsonDocument(message).toJson(QJsonDocument::Compact)+'\n';
    if(receive_.size()>256*1024) {fail("Request exceeds the session service size limit.");return;}
    Q_EMIT changed();deadline_.start();
    socket_.connectToServer(QDir(config_["state"].toString()).filePath("service.rpc"));
}
void UiSession::createTerminal(const QString &workspace) {request("terminal.create",workspace,"Shell");}
void UiSession::createWorkspace(const QString &name,const QString &directory) {request("workspace.create_at",{},name,directory);}
void UiSession::renameWorkspace(const QString &identity,const QString &name) {request("workspace.rename",identity,name);}
void UiSession::setWorkspaceDirectory(const QString &identity,const QString &directory) {request("workspace.directory",identity,{},directory);}
void UiSession::clearError() {if(!busy_ && !error_.isEmpty()) {error_.clear();Q_EMIT changed();}}
void UiSession::moveTerminal(const QString &identity,const QString &workspace,int position) {request("terminal.move",identity,{},{},workspace,position);}
QUrl UiSession::folderStart(const QString &value) const {
    // Resolve only when opening the picker, never in pane layout or rendering.
    QString path=value;
    if(path=="~")path=QDir::homePath();
    else if(path.startsWith("~/"))path=QDir::home().filePath(path.mid(2));
    else if(path.isEmpty())path=config_["cwd"].toString();
    if(QDir::isRelativePath(path))path=QDir(config_["cwd"].toString()).absoluteFilePath(path);
    path=QDir::cleanPath(path);
    while(!QFileInfo(path).isDir()) {
        const auto parent=QFileInfo(path).absolutePath();
        if(parent==path) {path=QDir::homePath();break;}
        path=parent;
    }
    return QUrl::fromLocalFile(path);
}
QString UiSession::folderPath(const QUrl &url) const {
    return url.isLocalFile() ? QDir::cleanPath(url.toLocalFile()) : QString();
}
void UiSession::refresh() {if(catalog_)catalog_->refresh();for(auto *facts:facts_)facts->refresh();request("initialize");}

void UiSession::reconnect(TerminalItem *item,const QString &identity,bool takeover) {
    static const QRegularExpression id("^[a-zA-Z0-9_-]{1,64}$");
    if(busy_ || !item || !id.match(identity).hasMatch())return;
    if(auto *facts=facts_.value(identity))facts->refresh();
    item->reconnectSession(QDir(config_["state"].toString()).filePath(identity+".ctl"),takeover);
}
void UiSession::recover(const QString &identity) {request("terminal.recover",identity);}
void UiSession::closeTerminal(const QString &identity) {request("terminal.close",identity);}
void UiSession::inspect(const QString &identity) {lifecycle_.inspect(identity);}

void UiSession::prune(const QJsonObject &catalog) {
    QSet<QString> active;
    for(const auto &workspace:catalog["workspaces"].toArray())for(const auto &terminal:workspace.toObject()["terminals"].toArray())active.insert(terminal.toObject()["id"].toString());
    for(const auto &id:facts_.keys())if(!active.contains(id))facts_.take(id)->deleteLater();
    lifecycle_.retain(active);
}
