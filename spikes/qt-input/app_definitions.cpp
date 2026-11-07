#include "app_definitions.h"
#include "app_bridge.h"
#include <QFile>
#include <QVariantMap>
#include <vector>
AppDefinitions AppDefinitions::read(const QString &path,bool apple)
{
    AppDefinitions result;char error[256]{};const auto encoded=QFile::encodeName(path);
    std::vector<WorkbenchApp> apps(32);
    const int count=workbench_app_definitions(path.isEmpty()?nullptr:encoded.constData(),apple,apps.data(),int(apps.size()),error,sizeof(error));
    if(count<0){result.error=QString::fromUtf8(error);return result;}
    for(int i=0;i<count;i++) {
        const auto &app=apps[size_t(i)];
        result.apps.append(QVariantMap{{"id",QString::fromUtf8(app.id)},{"name",QString::fromUtf8(app.name)},
            {"command",QString::fromUtf8(app.command)},{"directory",QString::fromUtf8(app.directory)},
            {"install",QString::fromUtf8(app.install)},{"setup",QString::fromUtf8(app.setup)}});
    }
    return result;
}
