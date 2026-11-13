import QtQuick
// A soft shadow under a floating surface (menus, dialogs, the command menu,
// tooltips): a child of the surface's background Rectangle, drawn behind it.
// Stacked translucent rounded rectangles, each a little larger, so it needs
// no effects module and costs a few flat quads.
Item {
    id: shadow
    property int spread: 20             // how far it reaches past the surface
    property real strength: 0.34        // its darkness right at the surface's edge
    readonly property int rings: 10
    anchors.fill: parent; z: -1
    Repeater {
        model: shadow.rings
        Rectangle {
            required property int index
            readonly property real grow: shadow.spread * (index + 1) / shadow.rings
            // Lit from above: the shadow hangs a third of its reach lower.
            x: -grow; y: -grow + shadow.spread / 3; width: shadow.width + 2 * grow; height: shadow.height + 2 * grow
            radius: (shadow.parent && shadow.parent.radius || 0) + grow
            color: Qt.rgba(0, 0, 0, shadow.strength / shadow.rings)
        }
    }
}
