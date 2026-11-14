#pragma once
#include <QQuickItem>
// Clears its rectangle to transparent, through everything painted below it in
// the scene: a transparent rectangle drawn without blending, so it replaces
// the pixels and, in the opaque pass, hides lower items by depth. Items above
// it paint as usual. Band uses it where the unpinned sidebar opens over the
// main area, so the system material behind the window shows there on macOS.
// It relies on the scene graph's depth buffer (the default renderer).
class WindowCutout : public QQuickItem {
    Q_OBJECT
public:
    explicit WindowCutout(QQuickItem *parent=nullptr):QQuickItem(parent){setFlag(ItemHasContents,true);}
protected:
    QSGNode *updatePaintNode(QSGNode *old,UpdatePaintNodeData *) override;
};
