#include "ui_catalog_feed.h"
#include "local_socket.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
namespace {
constexpr qsizetype limit=1024*1024;
// The service bounds (catalog.rs); a workspace may hold any share of the terminals.
constexpr qsizetype maxWorkspaces=64,maxTerminals=256;
bool name(const QJsonValue &value){if(!value.isString())return false;const auto text=value.toString();if(text.trimmed()!=text || text.isEmpty() || text.toUtf8().size()>63)return false;for(const auto c:text)if(c.unicode()<32 || c.unicode()==127)return false;return true;}
}
bool UiCatalogFeed::valid(const QJsonObject &catalog){
    if(catalog["version"].toDouble()!=1 || !catalog["workspaces"].isArray())return false;
    const auto workspaces=catalog["workspaces"].toArray();if(workspaces.size()>maxWorkspaces)return false;qsizetype total=0;
    static const QRegularExpression pattern("^[a-zA-Z0-9_-]{1,40}\\z");QSet<QString> ids;
    const auto entity=[&](const QJsonObject &value){const auto id=value["id"].toString();if(!pattern.match(id).hasMatch() || ids.contains(id) || !name(value["name"]))return false;ids.insert(id);return true;};
    for(const auto &item:workspaces){
        if(!item.isObject())return false;
        const auto workspace=item.toObject();if(!entity(workspace) || !workspace["terminals"].isArray() || (total+=workspace["terminals"].toArray().size())>maxTerminals)return false;
        if(workspace.contains("directory")){
            const auto folder=workspace["directory"];if(!folder.isString() || folder.toString().toUtf8().size()>1023)return false;
            for(const auto c:folder.toString())if(c.unicode()<32 || c.unicode()==127)return false;
        }
        for(const auto &terminal:workspace["terminals"].toArray())if(!terminal.isObject() || !entity(terminal.toObject()) || (terminal.toObject().contains("custom_name") && !terminal.toObject()["custom_name"].isBool()))return false;
    }
    return true;
}
UiCatalogFeed::UiCatalogFeed(const QString &directory,const QVariantMap &initial,QObject *parent,int timeoutMs,int settleMs)
    :QObject(parent),directory_(directory),path_(QDir(directory).filePath("workspaces.json")),endpoint_(QDir(directory).filePath("service.rpc")),known_(initial){
    socket_.setReadBufferSize(limit+1);settle_.setSingleShot(true);settle_.setInterval(settleMs);deadline_.setSingleShot(true);deadline_.setInterval(timeoutMs);
    connect(&watcher_,&QFileSystemWatcher::directoryChanged,this,[this]{schedule();});
    connect(&watcher_,&QFileSystemWatcher::fileChanged,this,[this]{schedule();});
    connect(&settle_,&QTimer::timeout,this,&UiCatalogFeed::read);
    connect(&deadline_,&QTimer::timeout,this,[this]{fail("Workspace updates timed out. Existing terminals remain connected.");});
    connect(&socket_,&QLocalSocket::connected,this,[this]{if(reading_)socket_.write("{\"protocol\":1,\"method\":\"catalog.read\"}\n");});
    connect(&socket_,&QLocalSocket::readyRead,this,[this]{
        if(!reading_)return;
        buffer_+=socket_.readAll();if(buffer_.size()>limit){fail("Workspace update exceeds its size limit.");return;}
        const auto newline=buffer_.indexOf('\n');if(newline<0)return;
        QJsonParseError parse;const auto document=QJsonDocument::fromJson(buffer_.left(newline),&parse);const auto reply=document.object();
        if(parse.error!=QJsonParseError::NoError || !document.isObject() || newline!=buffer_.size()-1 || reply["protocol"].toInt()!=1){fail("Invalid workspace update. Existing workspaces were kept.");return;}
        if(!reply["ok"].toBool()){
            const bool unsupported=reply["error"].toString()=="Unknown session service method";
            fail(unsupported?"Automatic workspace updates require a newer session service. Manual refresh remains available.":"Workspace updates unavailable. Existing workspaces were kept.",unsupported);return;
        }
        const auto catalog=reply["data"].toObject();if(!valid(catalog)){fail("Invalid workspace catalog. Existing workspaces were kept.");return;}
        reading_=false;deadline_.stop();socket_.abort();buffer_.clear();notice({});
        // If another invalidation arrived while reading, fetch the latest
        // snapshot before projecting a potentially obsolete intermediate one.
        if(dirty_){schedule();return;}
        const auto data=catalog.toVariantMap();if(data!=known_){known_=data;Q_EMIT updated(data);}
        Q_EMIT validated();
    });
    connect(&socket_,&QLocalSocket::disconnected,this,[this]{if(reading_)fail("Workspace update connection closed. Existing terminals remain connected.");});
    connect(&socket_,&QLocalSocket::errorOccurred,this,[this](auto){if(reading_)fail("Workspace updates unavailable. Existing terminals remain connected.");});
    watch();schedule();
}
UiCatalogFeed::~UiCatalogFeed(){socket_.disconnect(this);socket_.abort();}
void UiCatalogFeed::notice(const QString &message){if(error_!=message){error_=message;Q_EMIT changed();}}
void UiCatalogFeed::watch(){
    if(QFileInfo(directory_).isDir() && !watcher_.directories().contains(directory_))watcher_.addPath(directory_);
    if(QFileInfo::exists(path_) && !watcher_.files().contains(path_))watcher_.addPath(path_);
}
void UiCatalogFeed::schedule(){dirty_=true;if(!paused_ && !reading_ && !unsupported_)settle_.start();}
void UiCatalogFeed::pause(){paused_=true;reading_=false;settle_.stop();deadline_.stop();socket_.abort();buffer_.clear();}
void UiCatalogFeed::resume(const QVariantMap &confirmed){if(!confirmed.isEmpty())known_=confirmed;paused_=false;schedule();}
void UiCatalogFeed::refresh(){unsupported_=false;watch();schedule();}
void UiCatalogFeed::read(){
    if(paused_ || reading_ || unsupported_)return;
    dirty_=false;watch();
    if(!ownedLocalSocket(endpoint_)){notice("Workspace service unavailable. Existing terminals remain connected.");return;}
    reading_=true;buffer_.clear();deadline_.start();socket_.connectToServer(endpoint_);
}
void UiCatalogFeed::fail(const QString &message,bool unsupported){reading_=false;deadline_.stop();socket_.abort();buffer_.clear();unsupported_=unsupported;notice(message);if(dirty_ && !unsupported_)schedule();}
