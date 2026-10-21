#include "creation_defaults.h"
#include "creation_bridge.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>

CreationDefaults CreationDefaults::read(const QString &path,bool apple)
{
    CreationDefaults result;char folder[1024]{},message[256]{};double width=0.5;
    const auto encoded=QFile::encodeName(path);
    if(!workbench_creation_defaults(path.isEmpty()?nullptr:encoded.constData(),apple,folder,sizeof(folder),&width,message,sizeof(message))){result.error=QString::fromUtf8(message);return result;}
    QString directory=QString::fromUtf8(folder);
    if(directory=="~")directory=QDir::homePath();
    else if(directory.startsWith("~/"))directory=QDir::home().filePath(directory.mid(2));
    else if(directory.startsWith('~')){result.error="Workspace directory supports ~ or ~/path, without another user name.";return result;}
    if(!directory.isEmpty())directory=QDir::cleanPath(QDir(QFileInfo(path).absolutePath()).absoluteFilePath(directory));
    // Availability is checked by the session service on explicit Create. A
    // removable project folder must not prevent unrelated settings reloading.
    result.directory=directory;result.terminalWidth=width;return result;
}
