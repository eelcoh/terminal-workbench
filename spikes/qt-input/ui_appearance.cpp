#include "ui_appearance.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

UiAppearance::UiAppearance(const QString &path,bool apple,UiThemes &themes,QObject *parent,QString overrides)
    :QObject(parent),path_(path),overrides_(std::move(overrides)),apple_(apple),themes_(themes)
{
    const auto candidate=prepare();
    if(!adopt(candidate))error_="Using built-in workspace colors. "+(candidate.error.isEmpty()?error_:candidate.error);
}
QString UiAppearance::source(QString &error) const
{
    char value[1024]{},message[256]{};const auto path=QFile::encodeName(path_);
    if(!workbench_theme_default_source(path_.isEmpty()?nullptr:path.constData(),apple_,value,sizeof(value),message,sizeof(message)))error=QString::fromUtf8(message);
    return QString::fromUtf8(value);
}
QString UiAppearance::sourcePath() const
{
    QString error;const auto value=source(error);
    if(!error.isEmpty() || value.isEmpty())return {};
    for(int i=0;i<workbench_theme_builtin_count();++i)if(value==QString::fromUtf8(workbench_theme_name(i)))return {};
    if(value.startsWith("~/"))return QDir::homePath()+value.mid(1);
    return QDir(QFileInfo(path_).absolutePath()).absoluteFilePath(value);
}
bool UiAppearance::saved() const {return !overrides_.isEmpty() && (QFileInfo::exists(overrides_) || QFileInfo(overrides_).isSymLink());}
QVariantMap UiAppearance::savedSnapshot(QString &error) const
{
    if(!saved())return {};
    QFile file(overrides_);
    if(!QFileInfo(overrides_).isFile() || !file.open(QIODevice::ReadOnly)){error="Cannot read the default theme Band saved: "+overrides_;return {};}
    const auto bytes=file.read(ThemeStore::maxBytes*2+1);QJsonParseError parse;const auto document=QJsonDocument::fromJson(bytes,&parse);
    const auto theme=document.object()["default-theme"].toObject();
    const auto valid=parse.error==QJsonParseError::NoError && document.object()["version"].toInt()==1 && theme["name"].isString() && theme["colors"].isString()
        && !theme["name"].toString().isEmpty() && theme["name"].toString().toUtf8().size()<=120 && theme["colors"].toString().toUtf8().size()<=ThemeStore::maxBytes;
    if(!valid){error="The default theme Band saved is invalid; remove "+overrides_+" or choose a theme again.";return {};}
    return theme.toVariantMap();
}
UiAppearance::Candidate UiAppearance::prepare() const
{
    Candidate next;
    next.snapshot=savedSnapshot(next.error);
    if(!next.error.isEmpty() || !next.snapshot.isEmpty())return next;
    const auto value=source(next.error);
    if(!next.error.isEmpty() || value.isEmpty())return next;
    for(int i=0;i<workbench_theme_builtin_count();++i)if(value==QString::fromUtf8(workbench_theme_name(i))){next.builtin=i;return next;}
    next.builtin=-1;next.path=QDir(QFileInfo(path_).absolutePath()).absoluteFilePath(value);
    next.name=QFileInfo(next.path).fileName().simplified().left(30);
    if(!QFileInfo(next.path).isFile()){next.error="Configured workspace theme is not a readable file: "+next.path;return next;}
    QFile file(next.path);
    if(!file.open(QIODevice::ReadOnly)){next.error="Cannot read configured workspace theme: "+file.errorString();return next;}
    next.text=file.read(ThemeStore::maxBytes+1);
    if(file.error()!=QFileDevice::NoError){next.error="Cannot read configured workspace theme: "+file.errorString();return next;}
    WorkbenchTheme colors;int ignored=0;
    if(!UiThemes::parse(next.text,colors,ignored,next.error))return next;
    return next;
}
bool UiAppearance::adopt(const Candidate &next)
{
    if(!next.error.isEmpty())return false;
    if(!next.snapshot.isEmpty()) {
        const int index=themes_.restoreSnapshot(next.snapshot);
        if(index<0){error_="The default theme Band saved cannot be used. Choose a theme again.";return false;}
        index_=index;error_.clear();return true;
    }
    int index=next.builtin;
    if(index<0)index=themes_.registerColors(next.name,next.text,QUrl::fromLocalFile(next.path).toString());
    if(index<0){error_="Workspace theme registry is full. Reopen this window.";return false;}
    index_=index;error_.clear();return true;
}
bool UiAppearance::saveDefault(int index)
{
    const auto snapshot=themes_.snapshot(index);
    if(overrides_.isEmpty() || snapshot.isEmpty())return false;
    if(!QDir().mkpath(QFileInfo(overrides_).absolutePath())){error_="Cannot create the settings folder for "+overrides_;Q_EMIT changed();return false;}
    QJsonObject theme;for(const auto &key:{"name","colors","source","attribution"})theme.insert(key,snapshot.value(key).toString());
    QSaveFile file(overrides_);
    if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(QJsonObject{{"version",1},{"default-theme",theme}}).toJson())<0 || !file.commit()){error_="Cannot save the default theme: "+file.errorString();Q_EMIT changed();return false;}
    index_=index;error_.clear();Q_EMIT changed();return true;
}
bool UiAppearance::useConfigured()
{
    if(saved() && !QFile::remove(overrides_)){error_="Cannot remove "+overrides_;Q_EMIT changed();return false;}
    const auto next=prepare();
    if(!adopt(next)){index_=0;error_="Using built-in workspace colors. "+(next.error.isEmpty()?error_:next.error);}
    Q_EMIT changed();return true;
}
