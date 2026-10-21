#include "view_defaults.h"
#include "view_bridge.h"
#include <QFile>
ViewDefaults ViewDefaults::read(const QString &path,bool apple)
{
    ViewDefaults result;char error[256]{};const auto encoded=QFile::encodeName(path);
    if(!workbench_view_defaults(path.isEmpty()?nullptr:encoded.constData(),apple,&result.navigationPinned,&result.reducedMotion,error,sizeof(error)))result.error=QString::fromUtf8(error);
    return result;
}
