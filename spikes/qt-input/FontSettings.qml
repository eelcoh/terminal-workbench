import QtQuick
import QtQuick.Controls
import Band.Design
Dialog {
    id: popup
    required property var tones
    required property var preferences
    // Read when the dialog opens: listing installed fonts takes a moment.
    property var installed: []
    onAboutToShow: installed = preferences.installedFamilies
    property string installedQuery: ""
    readonly property var installedMatches: installed.filter(name => !installedQuery || name.toLocaleLowerCase().indexOf(installedQuery) >= 0)
    width: Math.min(480, parent.width - 64); padding: Design.space2xl; modal: true
    closePolicy: Popup.CloseOnEscape
    title: "Terminal text"
    background: Rectangle { color: popup.tones.raised; radius: Design.radiusXl; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: Design.space2xl; bottomPadding: Design.spaceM; color: popup.tones.foreground; font.pixelSize: Design.textTitle }
    contentItem: Column {
        spacing: Design.spaceXl
        Label { text: "Font"; color: popup.tones.muted; font.pixelSize: Design.textSmall }
        Row {
            width: parent.width; spacing: Design.spaceM
            ChromeButton { objectName: "fontJetBrains"; width: (parent.width - 8)/2; tones: popup.tones; text: "JetBrains Mono"; selected: popup.preferences.current.family === text; onClicked: popup.preferences.preview(text, popup.preferences.current.size) }
            ChromeButton { objectName: "fontFira"; width: (parent.width - 8)/2; tones: popup.tones; text: "Fira Code"; selected: popup.preferences.current.family === text; onClicked: popup.preferences.preview(text, popup.preferences.current.size) }
        }
        // Installed monospaced fonts, each shown in its own face.
        TextField {
            id: installedSearch; objectName: "fontInstalledSearch"; width: parent.width; height: 32
            placeholderText: popup.installed.length ? "Search " + popup.installed.length + " installed monospaced fonts" : "No other monospaced fonts installed"
            enabled: popup.installed.length > 0; color: popup.tones.foreground; placeholderTextColor: popup.tones.muted; font.pixelSize: Design.textBody
            leftPadding: Design.spaceM; selectByMouse: true
            background: Rectangle { radius: Design.radiusM; color: popup.tones.background; border.width: Design.line; border.color: installedSearch.activeFocus ? popup.tones.accent : popup.tones.border }
            onTextChanged: popup.installedQuery = text.trim().toLocaleLowerCase()
            Keys.onDownPressed: if (installedList.count) { installedList.forceActiveFocus(); installedList.currentIndex = 0; popup.preferences.preview(installedList.model[0], popup.preferences.current.size) }
        }
        ListView {
            id: installedList; objectName: "fontInstalledList"; width: parent.width; height: popup.installed.length ? 132 : 0; clip: true
            visible: height > 0; currentIndex: -1; boundsBehavior: Flickable.StopAtBounds
            model: popup.installedMatches
            ScrollBar.vertical: ScrollBar {}
            Keys.onReturnPressed: if (currentIndex >= 0) popup.preferences.preview(model[currentIndex], popup.preferences.current.size)
            Keys.onUpPressed: event => { if (currentIndex <= 0) installedSearch.forceActiveFocus(); else event.accepted = false }
            onCurrentIndexChanged: if (activeFocus && currentIndex >= 0) popup.preferences.preview(model[currentIndex], popup.preferences.current.size)
            delegate: Rectangle {
                id: fontRow; required property string modelData; required property int index
                readonly property bool chosen: popup.preferences.current.family === modelData
                objectName: "fontInstalled" + index; width: installedList.width; height: 30; radius: Design.radiusS
                color: chosen ? popup.tones.selected : rowHover.hovered || (installedList.activeFocus && installedList.currentIndex === index) ? popup.tones.hover : "transparent"
                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                Text { x: Design.spaceM; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 2 * Design.spaceM; elide: Text.ElideRight; text: fontRow.modelData; textFormat: Text.PlainText; font.family: fontRow.modelData; font.pixelSize: Design.textBody; color: popup.tones.foreground }
                TapHandler { onTapped: { installedList.currentIndex = fontRow.index; popup.preferences.preview(fontRow.modelData, popup.preferences.current.size) } }
            }
            Label { anchors.centerIn: parent; visible: installedList.count === 0; text: "No matching fonts"; color: popup.tones.muted; font.pixelSize: Design.textSmall }
        }
        Label { text: "Size"; color: popup.tones.muted; font.pixelSize: Design.textSmall }
        Row {
            spacing: Design.spaceXl
            ChromeButton { objectName: "fontMinus"; tones: popup.tones; text: "−"; hint: "Decrease terminal text size"; enabled: popup.preferences.current.size > 10; onClicked: popup.preferences.step(-1) }
            Label { width: 60; height: 32; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; text: popup.preferences.current.size + " pt"; color: popup.tones.foreground }
            ChromeButton { objectName: "fontPlus"; tones: popup.tones; text: "+"; hint: "Increase terminal text size"; enabled: popup.preferences.current.size < 32; onClicked: popup.preferences.step(1) }
        }
        Label { width: parent.width; text: "Preview across this window. Apply saves the font and size for future launches on this platform; config.conf can name an installed font too."; wrapMode: Text.Wrap; color: popup.tones.muted; font.pixelSize: Design.textSmall }
        Label { width: parent.width; visible: text.length > 0; text: popup.preferences.editError; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: popup.tones.waiting; font.pixelSize: Design.textSmall }
    }
    // The reset on the left; the decision on the right, Apply highlighted.
    footer: Item {
        implicitHeight: 56
        ChromeButton { objectName: "fontDefaults"; x: Design.space2xl; anchors.verticalCenter: parent.verticalCenter; tones: popup.tones; text: "Use dotfile defaults"; onClicked: popup.preferences.useDefaults() }
        Row {
            anchors.right: parent.right; anchors.rightMargin: Design.space2xl; anchors.verticalCenter: parent.verticalCenter; spacing: Design.spaceM
            ChromeButton { id: cancelButton; tones: popup.tones; text: "Cancel"; onClicked: popup.close() }
            ChromeButton { objectName: "fontApply"; tones: popup.tones; primary: true; text: "Apply"; onClicked: if (popup.preferences.apply()) popup.close() }
        }
    }
    onOpened: { installedSearch.text = ""; installedList.currentIndex = -1; cancelButton.forceActiveFocus() }
    onClosed: popup.preferences.cancel()
}
