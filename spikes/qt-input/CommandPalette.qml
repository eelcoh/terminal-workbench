import QtQuick
import QtQuick.Controls

Dialog {
    id: commandPopup
    required property var tones
    required property var actions
    property string contextLabel: ""
    readonly property int currentResult: results.currentIndex
    readonly property bool searchFocused: search.activeFocus
    readonly property var matches: {
        const words = search.text.trim().toLowerCase().split(/\s+/).filter(word => word.length)
        const score = action => words.reduce((sum, word) => sum + (action.title.toLowerCase().indexOf(word) >= 0 ? 1 : 0), 0)
        return actions.filter(action => words.every(word => (action.title + " " + action.detail).toLowerCase().indexOf(word) >= 0)).sort((a, b) => score(b) - score(a))
    }
    signal chosen(string actionId)
    implicitWidth: 600; width: Math.min(implicitWidth, parent.width - 64)
    height: Math.min(490, parent.height - 64); padding: 16; modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: commandPopup.tones.raised; radius: 12; border.color: commandPopup.tones.border }
    onOpened: { search.clear(); results.currentIndex = 0; search.forceActiveFocus() }
    onMatchesChanged: results.currentIndex = 0
    function step(delta) { if (matches.length) { results.currentIndex = (results.currentIndex + delta + matches.length) % matches.length; results.positionViewAtIndex(results.currentIndex, ListView.Contain) } }
    function runCurrent() { const entry = matches[results.currentIndex]; if (entry && entry.available) chosen(entry.id) }
    header: Item {
        implicitHeight: 64
        Label { x: 20; y: 14; text: "Commands"; color: commandPopup.tones.foreground; font.pixelSize: 22 }
        Label { x: 20; y: 43; width: parent.width - 40; text: commandPopup.contextLabel; textFormat: Text.PlainText; elide: Text.ElideMiddle; color: commandPopup.tones.muted; font.pixelSize: 12 }
    }
    contentItem: Column {
        spacing: 10
        TextField {
            id: search; KeyNavigation.tab: results; KeyNavigation.backtab: doneButton; objectName: "commandSearch"; width: parent.width; height: 42; selectByMouse: true
            Accessible.name: "Search commands"; placeholderText: "Search commands…"; color: commandPopup.tones.foreground
            placeholderTextColor: commandPopup.tones.muted; selectionColor: commandPopup.tones.accent; selectedTextColor: commandPopup.tones.background; padding: 12
            background: Rectangle { color: commandPopup.tones.background; radius: 6; border.color: search.activeFocus ? commandPopup.tones.accent : commandPopup.tones.border }
            Keys.onUpPressed: event => { if (!inputMethodComposing) { commandPopup.step(-1); event.accepted = true } else event.accepted = false }
            Keys.onDownPressed: event => { if (!inputMethodComposing) { commandPopup.step(1); event.accepted = true } else event.accepted = false }
            onAccepted: commandPopup.runCurrent()
        }
        ListView {
            id: results; activeFocusOnTab: true; KeyNavigation.tab: doneButton; KeyNavigation.backtab: search; objectName: "commandResults"; width: parent.width; height: parent.height - search.height - parent.spacing
            clip: true; model: commandPopup.matches; spacing: 2; currentIndex: 0; boundsBehavior: Flickable.StopAtBounds
            Accessible.name: "Commands"; keyNavigationEnabled: false
            ScrollBar.vertical: ScrollBar {}
            Keys.onUpPressed: commandPopup.step(-1)
            Keys.onDownPressed: commandPopup.step(1)
            Keys.onReturnPressed: commandPopup.runCurrent()
            Keys.onEnterPressed: commandPopup.runCurrent()
            Keys.onSpacePressed: commandPopup.runCurrent()
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                objectName: "commandRow" + index; focusPolicy: Qt.NoFocus
                width: results.width; height: 58; highlighted: results.currentIndex === index
                // Unavailable results stay selectable so their explanation is readable.
                Accessible.name: modelData.title + (modelData.available ? "" : ". " + modelData.reason)
                onClicked: { results.currentIndex = index; commandPopup.runCurrent() }
                contentItem: Item {
                    Text { x: 8; y: 5; width: parent.width - shortcut.width - 24; elide: Text.ElideRight; text: modelData.title; textFormat: Text.PlainText; font.pixelSize: 14; color: commandPopup.tones.foreground; opacity: modelData.available ? 1 : 0.5 }
                    Text { x: 8; y: 27; width: parent.width - 16; elide: Text.ElideRight; text: modelData.available ? modelData.detail : modelData.reason; textFormat: Text.PlainText; font.pixelSize: 12; color: commandPopup.tones.muted }
                    Text { id: shortcut; anchors.right: parent.right; anchors.rightMargin: 8; y: 6; text: modelData.key; font.pixelSize: 12; color: commandPopup.tones.muted }
                }
                background: Rectangle { radius: 6; color: parent.highlighted || parent.hovered ? commandPopup.tones.hover : "transparent"; border.width: parent.visualFocus ? 1 : 0; border.color: commandPopup.tones.accent }
            }
            Label { anchors.centerIn: parent; visible: commandPopup.matches.length === 0; text: "No matching commands"; color: commandPopup.tones.muted }
        }
    }
    footer: Item {
        implicitHeight: 48
        Label { x: 20; anchors.verticalCenter: parent.verticalCenter; text: "↑ ↓ choose · Enter run · Esc close"; font.pixelSize: 12; color: commandPopup.tones.muted }
        ChromeButton { id: doneButton; Keys.onReturnPressed: commandPopup.close(); Keys.onEnterPressed: commandPopup.close(); KeyNavigation.tab: search; KeyNavigation.backtab: results; anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter; text: "Done"; tones: commandPopup.tones; onClicked: commandPopup.close() }
    }
}
