#include "ui_session.h"
#include "theme_bridge.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

UiSession::UiSession(const QJsonObject &config,QObject *parent):QObject(parent),config_(config) {
    const auto color=[](WorkbenchThemeColor c){return QColor(c.r,c.g,c.b);};
    const auto mix=[](QColor a,QColor b,qreal amount){return QColor::fromRgbF(a.redF()*(1-amount)+b.redF()*amount,a.greenF()*(1-amount)+b.greenF()*amount,a.blueF()*(1-amount)+b.blueF()*amount);};
    for(int i=0;i<workbench_theme_count();++i) {
        WorkbenchTheme theme;if(!workbench_theme_get(i,&theme))continue;
        const auto bg=color(theme.background),fg=color(theme.foreground);QVariantList swatches;
        for(int j=0;j<8;++j)swatches.append(color(theme.palette[j]).name());
        themes_.append(QVariantMap{{"index",i},{"name",QString::fromUtf8(workbench_theme_name(i))},{"light",theme.light},
            {"background",bg.name()},{"foreground",fg.name()},{"accent",color(theme.palette[4]).name()},
            {"surface",mix(bg,fg,.06).name()},{"hover",mix(bg,fg,.12).name()},{"border",mix(bg,fg,.22).name()},
            {"muted",mix(bg,fg,.65).name()},{"swatches",swatches}});
    }
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
        if(!reply["data"].isObject() || !reply["data"].toObject()["workspaces"].isArray()) {fail("Missing session catalog. Refresh before trying again.");return;}
        const auto catalog=reply["data"].toObject().toVariantMap();const auto method=method_;
        deadline_.stop();busy_=false;socket_.abort();receive_.clear();Q_EMIT changed();Q_EMIT completed(catalog,method);
    });
    connect(&socket_,&QLocalSocket::disconnected,this,[this]{if(busy_)fail("Session service closed before replying. Refresh before trying again.");});
    connect(&socket_,&QLocalSocket::errorOccurred,this,[this](auto){if(busy_)fail(sent_?"Request outcome is unknown. Refresh before trying again.":socket_.errorString());});
}
void UiSession::fail(const QString &message) {
    busy_=false;deadline_.stop();socket_.abort();receive_.clear();error_=message;Q_EMIT changed();
}
void UiSession::attach(TerminalItem *item,const QString &identity) {
    static const QRegularExpression id("^[a-zA-Z0-9_-]{1,64}$");
    if(item && id.match(identity).hasMatch())item->connectSession(QDir(config_["state"].toString()).filePath(identity+".ctl"));
}
void UiSession::request(const QString &method,const QString &identity,const QString &name,const QString &directory,const QString &destination,int position) {
    if(busy_)return;
    socket_.abort();sent_=false;busy_=true;error_.clear();method_=method;
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
void UiSession::refresh() {request("initialize");}
