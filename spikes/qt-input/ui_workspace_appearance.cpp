#include "ui_workspace_appearance.h"
#include "ui_view_state.h"
#include "ui_themes.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
constexpr qint64 limit=80*1024;
bool identity(const QString &id){static const QRegularExpression pattern("^[a-zA-Z0-9_-]{1,64}$");return pattern.match(id).hasMatch();}
struct Read {QByteArray source="missing";QVariantMap value;QString error;};
Read readChoice(const QString &path){
    Read result;const QFileInfo info(path);
    if(!info.exists() && !info.isSymLink())return result;
    if(!info.isFile()){result.error="Appearance preference is not a regular file.";return result;}
    QFile file(path);if(!file.open(QIODevice::ReadOnly)){result.error="Cannot read workspace appearance: "+file.errorString();return result;}
    const auto bytes=file.read(limit+1);result.source="file:"+bytes;
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(bytes,&parse);
    if(file.error()!=QFileDevice::NoError || bytes.size()>limit || parse.error!=QJsonParseError::NoError || !document.isObject() || !UiWorkspaceAppearance::validChoice(document.object().toVariantMap())){result.error="Workspace appearance is invalid; file left untouched.";return result;}
    result.value=document.object().toVariantMap();return result;
}
}
UiWorkspaceAppearance::UiWorkspaceAppearance(const QString &directory,bool watch,QObject *parent)
    :QObject(parent),directory_(directory),watching_(watch){
    debounce_.setSingleShot(true);debounce_.setInterval(150);
    connect(&watcher_,&QFileSystemWatcher::fileChanged,this,[this]{debounce_.start();});
    connect(&watcher_,&QFileSystemWatcher::directoryChanged,this,[this]{debounce_.start();});
    connect(&debounce_,&QTimer::timeout,this,&UiWorkspaceAppearance::reload);
}
QString UiWorkspaceAppearance::error() const{return editError_.isEmpty()?QStringList(errors_.values()).join(" "):editError_;}
bool UiWorkspaceAppearance::validChoice(const QVariantMap &value){
    const auto root=QJsonObject::fromVariantMap(value);
    if(root["version"].toDouble(-1)!=1 || !root["default"].isBool())return false;
    if(root["default"].toBool())return root.size()==2;
    if(root.size()!=3 || !root["theme"].isObject())return false;
    const auto theme=root["theme"].toObject().toVariantMap();
    if(!UiViewState::validTheme(theme) || theme["name"].toString().trimmed().isEmpty())return false;
    for(const auto ch:theme["name"].toString())if(ch.unicode()<32 || ch.unicode()==127)return false;
    WorkbenchTheme colors;int ignored=0;QString error;
    return UiThemes::parse(theme["colors"].toString().toUtf8(),colors,ignored,error) && !ignored;
}
void UiWorkspaceAppearance::setWorkspaces(const QStringList &ids){
    if(ids.size()>8)return;
    QStringList seen;for(const auto &id:ids){if(!identity(id) || seen.contains(id))return;seen.append(id);}
    if(ids_==ids)return;
    ids_=ids;for(const auto &id:choices_.keys())if(!ids_.contains(id))choices_.remove(id);
    for(const auto &id:errors_.keys())if(!ids_.contains(id))errors_.remove(id);
    if(!ids_.contains(editing_)){editing_.clear();editError_.clear();}
    reload();Q_EMIT changed();
}
void UiWorkspaceAppearance::watchSources(){
    const auto files=watcher_.files(),directories=watcher_.directories();
    if(!files.isEmpty())watcher_.removePaths(files);
    if(!directories.isEmpty())watcher_.removePaths(directories);
    QStringList paths;
    for(const auto &id:ids_){
        const QFileInfo info(QDir(directory_).filePath(id+".json"));if(info.isFile())paths.append(info.absoluteFilePath());
        QStringList targets{info.absoluteFilePath()};if(info.isSymLink())targets.append(info.symLinkTarget());
        const auto resolved=info.canonicalFilePath();if(!resolved.isEmpty())targets.append(resolved);
        for(const auto &target:targets){QDir parent(QFileInfo(target).absolutePath());while(!parent.exists() && parent.cdUp()){}if(parent.exists())paths.append(parent.absolutePath());}
    }
    paths.removeDuplicates();if(!paths.isEmpty())watcher_.addPaths(paths);
}
void UiWorkspaceAppearance::reload(){
    if(watching_)watchSources();
    const auto before=choices_;const auto oldError=error();
    for(const auto &id:ids_){
        if(id==editing_)continue; // Never replace an open workspace's draft/baseline.
        const auto read=readChoice(QDir(directory_).filePath(id+".json"));
        if(!read.error.isEmpty()){errors_[id]=id+": "+read.error;continue;}
        errors_.remove(id);if(read.value.isEmpty())choices_.remove(id);else choices_[id]=read.value;
    }
    if(before!=choices_ || oldError!=error())Q_EMIT changed();
    if(oldError!=error() && !error().isEmpty())Q_EMIT reloadFailed();
}
bool UiWorkspaceAppearance::begin(const QString &id){
    if(!ids_.contains(id))return false;
    end();const auto read=readChoice(QDir(directory_).filePath(id+".json"));
    // The visible baseline and comparison bytes must come from the same read.
    if(read.error.isEmpty()){errors_.remove(id);if(read.value.isEmpty())choices_.remove(id);else choices_[id]=read.value;}
    else errors_[id]=id+": "+read.error;
    editing_=id;editSource_=read.source;editError_.clear();Q_EMIT changed();return true;
}
void UiWorkspaceAppearance::end(){editing_.clear();editError_.clear();reload();Q_EMIT changed();}
bool UiWorkspaceAppearance::fail(const QString &message){editError_=message;Q_EMIT changed();Q_EMIT reloadFailed();return false;}
bool UiWorkspaceAppearance::apply(const QString &id,bool follow,const QVariantMap &theme){
    if(editing_!=id || !ids_.contains(id))return fail("Workspace changed. Cancel and reopen Appearance.");
    QVariantMap value{{"version",1},{"default",follow}};if(!follow)value["theme"]=theme;
    if(!validChoice(value))return fail("Workspace colors could not be saved: invalid palette.");
    if(!QDir().mkpath(directory_))return fail("Cannot create workspace appearance folder.");
    const auto path=QDir(directory_).filePath(id+".json");QLockFile lock(path+".lock");
    if(!lock.tryLock(0))return fail("Workspace appearance is being saved in another window. Try again.");
    const auto current=readChoice(path);
    if(!current.error.isEmpty())return fail(current.error+" Repair it before saving.");
    if(current.source!=editSource_)return fail("Workspace appearance changed in another window. Cancel and reopen Appearance.");
    const auto encoded=QJsonDocument(QJsonObject::fromVariantMap(value)).toJson();QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) || file.write(encoded)!=encoded.size() || !file.commit())return fail("Cannot save workspace appearance: "+file.errorString());
    choices_[id]=value;errors_.remove(id);editSource_="file:"+encoded;editError_.clear();
    if(watching_)watchSources();
    Q_EMIT changed();return true;
}
