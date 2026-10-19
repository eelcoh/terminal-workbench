import QtQuick
import QtQuick.Controls
Popup {
    id: popup
    required property var tones
    signal finished()
    property var controller: null
    onControllerChanged: if(visible && !controller)close()
    readonly property var result: controller ? controller.state : ({})
    readonly property bool queryFocused: query.activeFocus
    modal: true; dim: false; focus: true; padding: 12
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    height: 92
    background: Rectangle {color: popup.tones.raised;radius: 10;border.color: popup.tones.border}
    onOpened: {query.text = "";query.forceActiveFocus(Qt.TabFocusReason)}
    onClosed: {if(controller)controller.close();controller = null;finished()}
    onResultChanged: if(visible && !result.active) close()
    component SearchButton: ChromeButton {
        Keys.onReturnPressed: event => {if(enabled)clicked();event.accepted = true}
        Keys.onEnterPressed: event => {if(enabled)clicked();event.accepted = true}
    }
    contentItem: Item {
        TextField {
            id: query;objectName: "terminalSearchQuery";x: 0;y: 0;width: parent.width - 204;height: 32
            placeholderText: "Find in terminal…";color: popup.tones.foreground;selectionColor: popup.tones.accent;selectedTextColor: popup.tones.background;placeholderTextColor: popup.tones.muted
            selectByMouse: true;maximumLength: 255
            background: Rectangle {color: popup.tones.background;radius: 6;border.color: query.activeFocus ? popup.tones.accent : popup.tones.border}
            onTextChanged: if(popup.controller)popup.controller.setQuery(text)
            Keys.onReturnPressed: event => {if(popup.controller)popup.controller.move(event.modifiers & Qt.ShiftModifier ? -1 : 1);event.accepted = true}
            Keys.onEnterPressed: event => {if(popup.controller)popup.controller.move(event.modifiers & Qt.ShiftModifier ? -1 : 1);event.accepted = true}
            Keys.onUpPressed: event => {if(popup.controller)popup.controller.move(-1);event.accepted = true}
            Keys.onDownPressed: event => {if(popup.controller)popup.controller.move(1);event.accepted = true}
            Accessible.name: "Find literal text in this terminal"
        }
        Label {x: query.width + 8;y: 8;width: 76;text: popup.result.busy ? "Searching…" : (popup.result.selected || 0) + "/" + (popup.result.count || 0) + (popup.result.limited ? "+" : "");elide: Text.ElideRight;color: popup.tones.muted;font.pixelSize: 12}
        Row {anchors.right: parent.right;spacing: 2
            SearchButton {objectName: "terminalSearchPrevious";tones: popup.tones;width: 32;height: 32;text: "↑";hint: "Previous match";enabled: !popup.result.busy && popup.result.count > 0;onClicked: popup.controller.move(-1)}
            SearchButton {objectName: "terminalSearchNext";tones: popup.tones;width: 32;height: 32;text: "↓";hint: "Next match";enabled: !popup.result.busy && popup.result.count > 0;onClicked: popup.controller.move(1)}
            SearchButton {objectName: "terminalSearchClose";tones: popup.tones;width: 32;height: 32;iconName: "close";hint: "Close search and restore view";onClicked: popup.close()}
        }
        Label {x: 0;y: 42;width: parent.width;elide: Text.ElideRight;font.pixelSize: 11;color: popup.tones.muted
            text: popup.result.error || (popup.result.limited ? "First 1024 matches · Refine your query" : !query.text ? "Literal, case-sensitive · Current terminal and scrollback" : !popup.result.busy && !popup.result.count ? "No matches · Esc restores your view" : "Enter next · Shift+Enter previous · Esc restores your view")
        }
    }
}
