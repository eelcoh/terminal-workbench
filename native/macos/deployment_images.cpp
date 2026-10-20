// One-shot loader evidence for the installed-preview diagnostic.
#include "deployment_check.h"
#include <mach-o/dyld.h>

QStringList deploymentLoadedImages()
{
    QStringList images;
    const auto count = _dyld_image_count();
    for (uint32_t i = 0; i < count; ++i) {
        if (const auto *name = _dyld_get_image_name(i))
            images.append(QString::fromLocal8Bit(name));
    }
    images.removeDuplicates();
    images.sort();
    return images;
}
