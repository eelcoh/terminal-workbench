#include "window_cutout.h"
#include <QSGSimpleRectNode>
#include <QSGMaterial>

QSGNode *WindowCutout::updatePaintNode(QSGNode *old,UpdatePaintNodeData *) {
    auto *node=static_cast<QSGSimpleRectNode *>(old);
    if(!node)node=new QSGSimpleRectNode;
    node->setRect(boundingRect());
    // setColor turns blending on for a translucent color; without it the
    // transparent pixels replace what is there instead of leaving it.
    node->setColor(Qt::transparent);
    node->material()->setFlag(QSGMaterial::Blending,false);
    return node;
}
