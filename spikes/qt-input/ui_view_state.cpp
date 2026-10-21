#include "ui_view_state.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <cmath>
namespace {
constexpr qint64 limit=128*1024;
QByteArray read(const QString &path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.read(limit+1):QByteArray();}
bool identity(const QString &value){static const QRegularExpression id("^[a-zA-Z0-9_-]{1,64}$");return value.isEmpty() || id.match(value).hasMatch();}
bool number(const QJsonValue &value,double low,double high){const auto n=value.toDouble(-1);return value.isDouble() && std::isfinite(n) && n>=low && n<=high;}
bool keys(const QJsonObject &object,const QStringList &names){for(auto it=object.begin();it!=object.end();++it)if(!names.contains(it.key()))return false;return true;}
}
bool UiViewState::valid(const QVariantMap &state){
    const auto root=QJsonObject::fromVariantMap(state),window=root["window"].toObject();
    if(!keys(root,{"navigation_pinned","version","window","workspace","terminal","overview","agents","needs","motion","overview_y","workspaces","terminals"}) || root["version"].toDouble()!=1)return false;
    if(root.contains("navigation_pinned") && !root["navigation_pinned"].isBool())return false;
    if(!root["window"].isObject() || !keys(window,{"width","height"}) || !number(window["width"],680,16384) || !number(window["height"],440,16384))return false;
    for(const auto &key:{"workspace","terminal"})if(!root[key].isString() || !identity(root[key].toString()))return false;
    for(const auto &key:{"overview","agents","needs","motion"})if(!root[key].isBool())return false;
    if(!number(root["overview_y"],0,1e7) || !root["workspaces"].isObject() || !root["terminals"].isObject())return false;
    const auto workspaces=root["workspaces"].toObject(),terminals=root["terminals"].toObject();if(workspaces.size()>8 || terminals.size()>64)return false;
    for(auto it=workspaces.begin();it!=workspaces.end();++it){
        const auto value=it.value().toObject();if(it.key().isEmpty() || !identity(it.key()) || !it.value().isObject() || !keys(value,{"focus","x","theme","theme_default"}) || !value["focus"].isString() || !identity(value["focus"].toString()) || !number(value["x"],0,1e7) || !value["theme"].isObject())return false;
        if(value.contains("theme_default") && !value["theme_default"].isBool())return false;
        const auto theme=value["theme"].toObject();if(!keys(theme,{"name","colors","source","attribution"}))return false;
        for(const auto &key:{"name","colors","source","attribution"})if(!theme[key].isString())return false;
        if(theme["name"].toString().isEmpty() || theme["name"].toString().toUtf8().size()>120 || theme["colors"].toString().toUtf8().size()>65536 || theme["source"].toString().size()>2048 || theme["attribution"].toString().size()>1024)return false;
    }
    for(auto it=terminals.begin();it!=terminals.end();++it){const auto value=it.value().toObject();if(it.key().isEmpty() || !identity(it.key()) || !it.value().isObject() || !keys(value,{"width","full"}) || !number(value["width"],.25,1) || !value["full"].isBool())return false;}
    return true;
}
UiViewState::UiViewState(const QString &path,QObject *parent):QObject(parent),path_(path){
    if(path_.isEmpty())return;
    original_=read(path_);
    if(!QFileInfo::exists(path_) && !QFileInfo(path_).isSymLink())return;
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(original_,&parse);
    if(original_.size()>limit || parse.error!=QJsonParseError::NoError || !document.isObject() || !valid(document.object().toVariantMap())){invalidate("Saved Qt window view is invalid. Using defaults; file left untouched.");return;}
    initial_=document.object().toVariantMap();
}
bool UiViewState::save(const QVariantMap &state){
    if(!enabled() || !writable_)return false;
    const auto fail=[&](const QString &message){error_=message;Q_EMIT changed();return false;};
    if(!valid(state))return fail("Window view could not be saved: invalid client state.");
    const auto encoded=QJsonDocument(QJsonObject::fromVariantMap(state)).toJson(QJsonDocument::Compact);
    if(encoded.size()>limit)return fail("Window view exceeds its size limit.");
    if(read(path_)!=original_){invalidate("Saved window view changed externally. Reopen the window before saving.");return false;}
    if(encoded==original_)return true;
    if(!QDir().mkpath(QFileInfo(path_).absolutePath()))return fail("Cannot create window view folder.");
    QSaveFile file(path_);if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) || file.write(encoded)!=encoded.size() || !file.commit())return fail("Could not save window view: "+file.errorString());
    original_=encoded;if(!error_.isEmpty()){error_.clear();Q_EMIT changed();}return true;
}
