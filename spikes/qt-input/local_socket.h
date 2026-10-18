#pragma once
#include <QFile>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif
inline bool ownedLocalSocket(const QString &path) {
#ifdef Q_OS_UNIX
    struct stat info{};
    return ::lstat(QFile::encodeName(path).constData(),&info)==0 && S_ISSOCK(info.st_mode) && info.st_uid==::geteuid();
#else
    Q_UNUSED(path);return false;
#endif
}
