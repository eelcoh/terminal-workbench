#include "theme_store.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QLockFile>

namespace {
bool label(const QString &value,int limit,bool optional=false) {
    if((!optional && value.trimmed().isEmpty()) || value.toUtf8().size()>limit)return false;
    for(const auto ch:value)if(ch.unicode()<32 || ch.unicode()==127)return false;
    return true;
}
bool metadata(const StoredTheme &theme) {
    static const QRegularExpression id("^[a-f0-9]{64}$");
    return id.match(theme.id).hasMatch() && label(theme.name,120)
        && label(theme.source,4096) && label(theme.attribution,1024,true);
}
bool read(const QString &path,qint64 limit,QByteArray &data,QString &error) {
    const QFileInfo info(path);QFile file(path);
    if(info.isSymLink() || !info.isFile() || info.size()>limit || !file.open(QIODevice::ReadOnly)) {
        error="Could not read bounded theme data: "+info.fileName();return false;
    }
    data=file.read(limit+1);
    if(data.size()>limit || file.error()!=QFileDevice::NoError) {error="Theme data exceeds its size limit.";return false;}
    return true;
}
bool write(const QString &path,const QByteArray &data,QString &error) {
    if(QFileInfo(path).isSymLink()) {error="Theme store file is a symbolic link.";return false;}
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)
        || file.write(data)!=data.size() || !file.commit()) {error="Could not save theme data: "+file.errorString();return false;}
    return true;
}
}
QString ThemeStore::digest(const QByteArray &text) {return QString::fromLatin1(QCryptographicHash::hash(text,QCryptographicHash::Sha256).toHex());}
bool ThemeStore::load(const QString &directory,QVector<StoredTheme> &themes,QString &error) {
    error.clear();QVector<StoredTheme> next;const QDir folder(directory);
    if(!folder.isAbsolute()) {error="Theme store must have an absolute path.";return false;}
    const auto index=folder.filePath("imports.json");
    if(!QFileInfo::exists(index) && !QFileInfo(index).isSymLink()) {themes.clear();return true;}
    QByteArray bytes;if(!read(index,maxManifestBytes,bytes,error))return false;
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(bytes,&parse);const auto object=document.object();
    if(parse.error!=QJsonParseError::NoError || !document.isObject() || object["version"].toInt()!=1 || !object["imports"].isArray()) {
        error="Invalid saved theme index; existing files were preserved.";return false;
    }
    const auto entries=object["imports"].toArray();QSet<QString> seen;
    if(entries.size()>maxThemes) {error="Saved theme count exceeds the limit.";return false;}
    for(const auto &value:entries) {
        const auto item=value.toObject();StoredTheme theme{item["id"].toString(),item["name"].toString(),item["source"].toString(),item["attribution"].toString(),{}};
        if(!value.isObject() || !metadata(theme) || seen.contains(theme.id)) {error="Invalid saved theme metadata; existing files were preserved.";return false;}
        if(!read(folder.filePath(theme.id+".ghostty"),maxBytes,theme.text,error))return false;
        if(theme.text.isEmpty() || digest(theme.text)!=theme.id) {error="Saved theme data failed its checksum; existing files were preserved.";return false;}
        seen.insert(theme.id);next.append(theme);
    }
    themes=next;return true;
}
bool ThemeStore::save(const QString &directory,const QVector<StoredTheme> &themes,QString &error) {
    error.clear();const QDir folder(directory);QSet<QString> seen;QJsonArray entries;
    if(!folder.isAbsolute() || themes.size()>maxThemes) {error="Invalid theme store path or count.";return false;}
    // Validate the entire transaction before creating any files.
    for(const auto &theme:themes) {
        if(!metadata(theme) || seen.contains(theme.id) || theme.text.isEmpty() || theme.text.size()>maxBytes || digest(theme.text)!=theme.id) {error="Invalid theme save transaction.";return false;}
        seen.insert(theme.id);entries.append(QJsonObject{{"id",theme.id},{"name",theme.name},{"source",theme.source},{"attribution",theme.attribution}});
    }
    const auto manifest=QJsonDocument(QJsonObject{{"version",1},{"imports",entries}}).toJson();
    if(manifest.size()>maxManifestBytes) {error="Theme index exceeds the size limit.";return false;}
    const bool existed=folder.exists();
    if(!QDir().mkpath(directory) || (!existed && !QFile::setPermissions(directory,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner))) {error="Could not create the theme store.";return false;}
    for(const auto &theme:themes) {
        const auto path=folder.filePath(theme.id+".ghostty");
        if(QFileInfo::exists(path) || QFileInfo(path).isSymLink()) {
            QByteArray old;if(!read(path,maxBytes,old,error))return false;
            if(old!=theme.text) {error="Existing theme data differs; it was preserved.";return false;}
        } else if(!write(path,theme.text,error))return false;
    }
    return write(folder.filePath("imports.json"),manifest,error);
}

bool ThemeStore::insert(const QString &directory,const StoredTheme &theme,QVector<StoredTheme> &merged,QString &error,const std::function<bool(const QByteArray &,QString &)> &validate) {
    const QDir folder(directory);const bool existed=folder.exists();
    if(!folder.isAbsolute() || !QDir().mkpath(directory) || (!existed && !QFile::setPermissions(directory,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner))) {error="Could not open the theme store.";return false;}
    QLockFile lock(folder.filePath("import.lock"));
    if(!lock.tryLock(0)) {error="Another window is importing a theme. Try again shortly.";return false;}
    QVector<StoredTheme> current;
    if(!load(directory,current,error))return false;
    for(const auto &record:current)if(!validate(record.text,error))return false;
    for(const auto &record:current)if(record.id==theme.id) {merged=current;return true;}
    if(current.size()>=maxThemes) {error="The imported theme limit is 64.";return false;}
    if(!validate(theme.text,error))return false;
    current.append(theme);
    if(!save(directory,current,error))return false;
    merged=current;return true;
}
