import QtQuick
import QtQuick.Controls
import Band.Design
Menu {
    id: menu
    property var tones
    padding: Design.spaceS; implicitWidth: 280
    background: Rectangle { color: menu.tones.raised; radius: Design.radiusXl; border.color: menu.tones.border; SurfaceShadow {} }
}
