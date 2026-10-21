#include "ui_appearance.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

UiAppearance::UiAppearance(const QString &path,bool apple,UiThemes &themes,QObject *parent)
    :QObject(parent),path_(path),apple_(apple),themes_(themes)
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
    return QDir(QFileInfo(path_).absolutePath()).absoluteFilePath(value);
}
UiAppearance::Candidate UiAppearance::prepare() const
{
    Candidate next;const auto value=source(next.error);
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
    int index=next.builtin;
    if(index<0)index=themes_.registerColors(next.name,next.text,QUrl::fromLocalFile(next.path).toString());
    if(index<0){error_="Workspace theme registry is full. Reopen this window.";return false;}
    index_=index;error_.clear();return true;
}
