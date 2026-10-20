// Read only this diagnostic process, never user terminals or process trees.
#include "deployment_check.h"
#include <QFile>

QStringList deploymentLoadedImages()
{
    QStringList images;
    QFile maps(QStringLiteral("/proc/self/maps"));
    if (!maps.open(QIODevice::ReadOnly)) return images;
    const auto lines = maps.readAll().split('\n');
    for (const auto &line : lines) {
        const auto start = line.indexOf('/');
        if (start >= 0) images.append(QString::fromLocal8Bit(line.mid(start)));
    }
    images.removeDuplicates();
    images.sort();
    return images;
}
