import QtQuick
import QtQuick.Controls
import Band.Design

// Settings in cards (owner, 2026-11-12, after Rex): a section's rows on a soft
// rounded panel with hairlines between them, under an optional heading.
Column {
    id: card
    required property var tones
    property string title: ""
    default property alias rows: panel.data
    width: parent ? parent.width : 0
    spacing: Design.spaceS
    Label {
        visible: card.title.length > 0; width: card.width; leftPadding: Design.spaceXs
        text: card.title; textFormat: Text.PlainText; wrapMode: Text.Wrap
        font.pixelSize: Design.textBody; font.weight: Font.DemiBold; color: card.tones.foreground
    }
    Rectangle {
        width: card.width; height: panel.height
        radius: Design.radiusL; color: Design.withAlpha(card.tones.foreground, 0.045)
        Column { id: panel; width: parent.width }
    }
}
