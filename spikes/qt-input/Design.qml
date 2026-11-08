pragma Singleton
import QtQuick
// Band's design values: every component reads its type sizes, spacing, corner
// radii, lines and timings from here (import Band.Design). Colors are not here:
// they come from the terminal theme (UiThemes, passed down as `tones`).
// Changing a value here changes it everywhere. See docs/design.md.
QtObject {
    // Type, in pixels. One step per role; the comments say what each is for.
    // On macOS every step is a pixel smaller: the same sizes read large there.
    // macOS, whichever name this Qt reports for it.
    readonly property bool apple: Qt.platform.os === "osx" || Qt.platform.os === "macos"
    readonly property int textShift: apple ? -1 : 0
    readonly property int textCaption: 11 + textShift   // badges, key chips, counts, sidebar details
    readonly property int textSmall: 12 + textShift     // secondary lines, hints, captions
    readonly property int textBody: 13 + textShift      // list rows, field text, explanations
    readonly property int textControl: 14 + textShift   // buttons, menu items, pane names
    readonly property int textHeading: 16 + textShift   // a heading inside a view
    readonly property int textTitle: 20 + textShift     // dialog and panel titles
    readonly property int textDisplay: 28 + textShift   // the welcome

    // Space between and around things, in pixels.
    readonly property int space2xs: 2
    readonly property int spaceXs: 4
    readonly property int spaceS: 6
    readonly property int spaceM: 8
    readonly property int spaceL: 12
    readonly property int spaceXl: 16
    readonly property int space2xl: 20
    readonly property int space3xl: 24

    // Corner radii. Dots, badges and pills stay round: half their height.
    readonly property int radiusS: 4        // chips, small marks, focus rings inside rows
    readonly property int radiusM: 6        // buttons, rows, fields
    readonly property int radiusL: 8        // cards and swatches
    readonly property int radiusXl: 12      // dialogs, popups, menus

    // Lines.
    readonly property int line: 1           // borders and separators
    readonly property int lineStrong: 2     // keyboard focus on a main action, the current pane

    // Motion, in milliseconds; zero under reduced motion is decided by the caller.
    readonly property int motionQuick: 120  // fades
    readonly property int motionMove: 220   // the strip, the overview
}
