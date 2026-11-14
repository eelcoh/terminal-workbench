import QtQuick
import QtQuick.Controls
import Band.Design
Menu {
    id: menu
    property var tones
    padding: Design.spaceS; implicitWidth: 280
    // On macOS the menu is the system's own (NSMenu): its shape, animation and
    // keyboard handling. Items then show their text, check mark and enabled
    // state; key hints and Band's tones belong to the drawn menu elsewhere.
    Binding { target: menu; property: "popupType"; value: Popup.Native; when: Design.apple }
    background: Rectangle { color: menu.tones.raised; radius: Design.radiusXl; border.color: menu.tones.border; SurfaceShadow {} }
}
