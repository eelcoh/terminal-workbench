import QtQuick
import QtQuick.Controls
import Band.Design

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
    // A search field over a list, as in Rex: no title, legend or Done button.
    // Escape and a click outside close it; the context shows in the field.
    implicitWidth: 600; width: Math.min(implicitWidth, parent.width - 64)
    height: Math.min(420, parent.height - 64); padding: 0; modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: commandPopup.tones.raised; radius: Design.radiusXl; border.width: Design.line; border.color: commandPopup.tones.border; SurfaceShadow {} }
    onOpened: { search.clear(); results.currentIndex = 0; search.forceActiveFocus() }
    onMatchesChanged: results.currentIndex = 0
    function step(delta) { if (matches.length) { results.currentIndex = (results.currentIndex + delta + matches.length) % matches.length; results.positionViewAtIndex(results.currentIndex, ListView.Contain) } }
    function runCurrent() { const entry = matches[results.currentIndex]; if (entry && entry.available) chosen(entry.id) }
    contentItem: Column {
        spacing: 0
        TextField {
            id: search; KeyNavigation.tab: results; KeyNavigation.backtab: results; objectName: "commandSearch"; width: parent.width; height: 52; selectByMouse: true
            Accessible.name: "Search commands"; Accessible.description: "Up and Down choose, Enter runs, Escape closes"
            placeholderText: "Search commands" + (commandPopup.contextLabel ? " · " + commandPopup.contextLabel : "")
            color: commandPopup.tones.foreground; font.pixelSize: Design.textTitle - 2
            placeholderTextColor: commandPopup.tones.muted; selectionColor: commandPopup.tones.accent; selectedTextColor: commandPopup.tones.background
            leftPadding: Design.spaceXl; rightPadding: Design.spaceXl
            background: Item {}
            Keys.onUpPressed: event => { if (!inputMethodComposing) { commandPopup.step(-1); event.accepted = true } else event.accepted = false }
            Keys.onDownPressed: event => { if (!inputMethodComposing) { commandPopup.step(1); event.accepted = true } else event.accepted = false }
            onAccepted: commandPopup.runCurrent()
        }
        Rectangle { width: parent.width; height: Design.line; color: commandPopup.tones.border }
        ListView {
            id: results; activeFocusOnTab: true; KeyNavigation.tab: search; KeyNavigation.backtab: search; objectName: "commandResults"
            x: Design.spaceS; width: parent.width - 2 * Design.spaceS; height: parent.height - search.height - Design.line
            topMargin: Design.spaceS; bottomMargin: Design.spaceS
            clip: true; model: commandPopup.matches; spacing: Design.space2xs; currentIndex: 0; boundsBehavior: Flickable.StopAtBounds
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
                width: results.width; height: 32; highlighted: results.currentIndex === index
                // Unavailable results stay selectable so their explanation is readable.
                Accessible.name: modelData.title + (modelData.available ? "" : ". " + modelData.reason)
                Accessible.description: modelData.key
                onClicked: { results.currentIndex = index; commandPopup.runCurrent() }
                // One line (owner, 2026-11-12): the title, then muted after it the
                // reason a command is unavailable, or the highlighted row's detail;
                // the shortcut as plain glyphs on the right.
                contentItem: Item {
                    Text {
                        id: commandTitle; x: 4; anchors.verticalCenter: parent.verticalCenter; width: Math.min(implicitWidth, parent.width - shortcut.width - 24)
                        elide: Text.ElideRight; text: modelData.title; textFormat: Text.PlainText; font.pixelSize: Design.textBody; color: commandPopup.tones.foreground; opacity: modelData.available ? 1 : 0.5
                    }
                    Text {
                        anchors.left: commandTitle.right; anchors.leftMargin: Design.spaceM; anchors.baseline: commandTitle.baseline
                        width: Math.max(0, parent.width - commandTitle.width - shortcut.width - 4 - 2 * Design.spaceM - Design.spaceXs)
                        elide: Text.ElideRight; text: !modelData.available ? modelData.reason : parent.parent.highlighted ? modelData.detail : ""
                        textFormat: Text.PlainText; font.pixelSize: Design.textCaption; color: commandPopup.tones.muted
                    }
                    KeyChips { id: shortcut; anchors.right: parent.right; anchors.rightMargin: Design.spaceXs; anchors.verticalCenter: parent.verticalCenter; keys: modelData.key; tones: commandPopup.tones; plain: true }
                }
                background: Rectangle { radius: Design.radiusM; color: parent.highlighted ? commandPopup.tones.selected : parent.hovered ? commandPopup.tones.hover : "transparent"; border.width: parent.visualFocus ? Design.line : 0; border.color: commandPopup.tones.accent }
            }
            Label { anchors.centerIn: parent; visible: commandPopup.matches.length === 0; text: "No matching commands"; color: commandPopup.tones.muted }
        }
    }
}
