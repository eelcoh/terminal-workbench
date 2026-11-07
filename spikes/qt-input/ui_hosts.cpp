#include "ui_hosts.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QProcess>
#include <QSaveFile>
#include <QTcpSocket>
#include <QTimer>

UiHosts::UiHosts(QString path,QString sshConfig,QObject *parent,QString ssh):QObject(parent),path_(std::move(path)),sshConfig_(std::move(sshConfig)),ssh_(std::move(ssh)) {
    // Other windows save too: follow the file and its folder (for creation).
    connect(&watcher_,&QFileSystemWatcher::fileChanged,this,[this]{load();});
    connect(&watcher_,&QFileSystemWatcher::directoryChanged,this,[this]{load();});
    load();
}
bool UiHosts::validId(const QString &id) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9-]{1,34}$"));
    return pattern.match(id).hasMatch();
}
bool UiHosts::validDestination(const QString &destination) {
    // As in config.conf: an alias, host or user@host, never an ssh option.
    static const QRegularExpression pattern(QStringLiteral(R"(^[A-Za-z0-9._@:\[\]%][A-Za-z0-9._@:\[\]%-]{0,254}$)"));
    return pattern.match(destination).hasMatch();
}
static bool plain(const QString &text,int limit) {
    if(text.toUtf8().size()>=limit)return false;
    for(const auto ch:text)if(ch.category()==QChar::Other_Control)return false;
    return true;
}
void UiHosts::load() {
    if(path_.isEmpty())return;
    const QFileInfo info(path_);
    if(!watcher_.files().contains(path_) && info.exists())watcher_.addPath(path_);
    if(QDir().exists(info.absolutePath()) && !watcher_.directories().contains(info.absolutePath()))watcher_.addPath(info.absolutePath());
    QVariantList next;QString problem;
    if(info.exists()) {
        QFile file(path_);QJsonParseError parse;
        const auto document=file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.read(1<<20),&parse) : QJsonDocument();
        if(!file.isOpen() || parse.error!=QJsonParseError::NoError || document.object()["version"].toInt()!=1 || !document.object()["hosts"].isArray())problem="Saved hosts are unreadable: "+path_;
        else for(const auto &value:document.object()["hosts"].toArray()) {
            const auto host=value.toObject();
            const auto id=host["id"].toString(),destination=host["destination"].toString();
            if(!validId(id) || !validDestination(destination)){problem="Saved hosts contain an invalid entry; it is skipped.";continue;}
            next.append(QVariantMap{{"id",id},{"name",host["name"].toString().left(63).isEmpty()?id:host["name"].toString().left(63)},{"destination",destination},
                {"directory",host["directory"].toString().left(1023)},{"theme",host["theme"].toString().left(127)}});
        }
    }
    if(next==saved_ && problem==error_)return;
    saved_=next;error_=problem;Q_EMIT changed();
}
QVariantList UiHosts::merge(const QVariantList &configured) const {
    QVariantList result;
    for(const auto &item:configured) {
        const auto id=item.toMap().value("id").toString();bool replaced=false;
        for(const auto &own:saved_)if(own.toMap().value("id").toString()==id)replaced=true;
        if(!replaced)result.append(item);
    }
    for(const auto &own:saved_){auto host=own.toMap();host.insert("saved",true);result.append(host);}
    return result;
}
bool UiHosts::write(const QVariantList &hosts) {
    if(path_.isEmpty()){error_="Hosts cannot be saved in this window.";Q_EMIT changed();return false;}
    if(!QDir().mkpath(QFileInfo(path_).absolutePath())){error_="Cannot create "+QFileInfo(path_).absolutePath();Q_EMIT changed();return false;}
    QJsonArray array;for(const auto &host:hosts)array.append(QJsonObject::fromVariantMap(host.toMap()));
    QSaveFile file(path_);
    if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(QJsonObject{{"version",1},{"hosts",array}}).toJson())<0 || !file.commit()){error_="Cannot save hosts: "+file.errorString();Q_EMIT changed();return false;}
    saved_=hosts;error_.clear();Q_EMIT changed();load();return true;
}
bool UiHosts::save(const QVariantMap &input) {
    const auto id=input.value("id").toString().trimmed(),destination=input.value("destination").toString().trimmed();
    const auto name=input.value("name").toString().trimmed(),directory=input.value("directory").toString().trimmed();
    if(!validId(id)){error_="A host ID is letters, digits and dashes.";Q_EMIT changed();return false;}
    if(!validDestination(destination)){error_="Enter an ssh alias, host or user@host, without spaces or a leading dash.";Q_EMIT changed();return false;}
    if(!plain(name,64) || !plain(directory,1024)){error_="The name or folder is too long or contains control characters.";Q_EMIT changed();return false;}
    QVariantList next;
    for(const auto &host:saved_)if(host.toMap().value("id").toString()!=id)next.append(host);
    next.append(QVariantMap{{"id",id},{"name",name.isEmpty()?id:name},{"destination",destination},{"directory",directory},{"theme",input.value("theme").toString().left(127)}});
    return write(next);
}
bool UiHosts::remove(const QString &id) {
    QVariantList next;
    for(const auto &host:saved_)if(host.toMap().value("id").toString()!=id)next.append(host);
    return next.size()!=saved_.size() && write(next);
}
QString UiHosts::freeId(const QString &from,const QVariantList &taken) const {
    QString base=from.toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),QStringLiteral("-"));
    base=base.mid(base.indexOf(QRegularExpression(QStringLiteral("[a-z0-9]")))).left(30);
    while(base.endsWith('-'))base.chop(1);
    if(base.isEmpty())base="host";
    QStringList used;for(const auto &host:taken)used.append(host.toMap().value("id").toString());
    QString id=base;for(int n=2;used.contains(id);++n)id=base+"-"+QString::number(n);
    return id;
}
QStringList UiHosts::aliases(const QByteArray &sshConfig) {
    // Top-level Host lines only; patterns (*, ?, !) and Match blocks are skipped.
    QStringList result;
    for(const auto &raw:sshConfig.split('\n')) {
        const auto line=QString::fromUtf8(raw).trimmed();
        const auto fields=line.split(QRegularExpression(QStringLiteral("[\\s=]+")),Qt::SkipEmptyParts);
        if(fields.size()<2 || fields[0].compare("Host",Qt::CaseInsensitive)!=0)continue;
        for(int i=1;i<fields.size();++i) {
            const auto alias=fields[i];
            if(alias.startsWith('#'))break;
            if(alias.contains(QRegularExpression(QStringLiteral("[*?!]"))) || !validDestination(alias) || result.contains(alias))continue;
            result.append(alias);
        }
    }
    return result;
}
QStringList UiHosts::suggestions() const {
    QFile file(sshConfig_);
    if(sshConfig_.isEmpty() || !file.open(QIODevice::ReadOnly))return {};
    return aliases(file.read(1<<20));
}
QVariantMap UiHosts::resolved(const QByteArray &output) {
    QVariantMap result{{"port",22},{"proxied",false}};
    for(const auto &raw:output.split('\n')) {
        const auto line=QString::fromUtf8(raw).trimmed();const auto key=line.section(' ',0,0).toLower(),value=line.section(' ',1).trimmed();
        if(key=="hostname")result["host"]=value;
        else if(key=="port")result["port"]=value.toInt();
        else if((key=="proxyjump" || key=="proxycommand") && !value.isEmpty() && value!="none")result["proxied"]=true;
    }
    return result;
}
void UiHosts::mark(const QString &id,const QString &state) {
    if(state!="checking")pending_.remove(id);
    if(reachability_.value(id).toString()==state)return;
    reachability_.insert(id,state);Q_EMIT reachabilityChanged();
}
void UiHosts::check(const QVariantList &hosts) {
    for(const auto &item:hosts) {
        const auto host=item.toMap();const auto id=host.value("id").toString(),destination=host.value("destination").toString();
        if(!validDestination(destination) || pending_.contains(id))continue;
        pending_.insert(id);
        if(!reachability_.contains(id))mark(id,"checking");
        auto *resolve=new QProcess(this);
        resolve->setStandardInputFile(QProcess::nullDevice());
        connect(resolve,&QProcess::finished,resolve,&QObject::deleteLater);
        connect(resolve,&QProcess::errorOccurred,this,[this,resolve,id](QProcess::ProcessError error){if(error==QProcess::FailedToStart){mark(id,"unknown");resolve->deleteLater();}});
        connect(resolve,&QProcess::finished,this,[this,resolve,id](int code,QProcess::ExitStatus status){
            const auto target=resolved(resolve->readAllStandardOutput());
            const auto address=target.value("host").toString();const int port=target.value("port").toInt();
            if(code!=0 || status!=QProcess::NormalExit || target.value("proxied").toBool() || address.isEmpty() || port<1 || port>65535){mark(id,"unknown");return;}
            auto *socket=new QTcpSocket(this);auto *limit=new QTimer(socket);limit->setSingleShot(true);
            const auto finish=[this,socket,id](const QString &state){if(socket->property("done").toBool())return;socket->setProperty("done",true);mark(id,state);socket->abort();socket->deleteLater();};
            connect(socket,&QTcpSocket::connected,this,[finish]{finish("reachable");});
            connect(socket,&QTcpSocket::errorOccurred,this,[finish]{finish("unreachable");});
            connect(limit,&QTimer::timeout,this,[finish]{finish("unreachable");});
            limit->start(2000);socket->connectToHost(address,quint16(port));
        });
        // A stuck ssh -G (a slow resolver in its config) is stopped after five seconds.
        QTimer::singleShot(5000,resolve,[resolve]{resolve->kill();});
        resolve->start(ssh_,{"-G","--",destination});
    }
}
