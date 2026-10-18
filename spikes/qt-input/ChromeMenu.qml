import QtQuick
import QtQuick.Controls
Menu {
    id: menu
    property var tones
    padding: 6; implicitWidth: 280
    background: Rectangle { color: menu.tones.raised; radius: 12; border.color: menu.tones.border }
}
