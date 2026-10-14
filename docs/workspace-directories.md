# Workspace project folders

A workspace can save a project folder shared by all windows. In **New workspace**,
enter a name and a Project folder; Tab visits the fields, Cancel and Create; Shift+Tab moves backward. For an
existing workspace, use **Folder** beside Rename in the sidebar. Type or paste an
absolute path, `~/...`, or a path relative to the Workbench project directory.
The focused field has an outline; selected text uses the theme's selection colors.
Existing names and paths open with the caret at the end, retaining the full value.
Click to place the caret or drag to select part of the text; double-click selects
a word and triple-click selects the whole value. Arrows move the caret; Shift
extends selection. Home/End reach the start/end, and Ctrl+arrows on Linux or
Option+arrows on Mac move by word. Mac Command+Left/Right also reach the start/end.
Backspace and Delete remove characters or selected text. Long paths scroll to
keep the caret visible. Each field keeps its own position when using Tab.

Copy, cut, paste and Select all use Ctrl+C/X/V/A on Linux and Command+C/X/V/A on
Mac, independently of terminal shortcuts. To replace an entire existing value,
use Select all explicitly. Generated names for **New workspace/terminal** start
selected. Rejected multi-line, malformed UTF-8 or oversized paste preserves the
existing value and selection. Escape/Cancel discards edits. A native folder
browser remains follow-up.

The spike currently supports eight workspaces. At capacity, New workspace changes
to **Workspace limit (8)**; hovering or clicking explains that closing a workspace
allows another to be created.

The first shell in a new workspace, subsequent new terminals, and explicitly
recovered shells start in its saved folder. Existing shells retain their current
working directory, jobs, PID and terminal state when the setting changes.
Moving a live terminal to another workspace also preserves its current directory;
its next replacement shell starts in the destination workspace's folder.

Paths are resolved to absolute paths when set. Spaces and UTF-8 are supported;
control characters, paths over 1023 UTF-8 bytes, missing folders and inaccessible
folders are rejected before the operation saves or starts a shell. Saved paths
need not exist when loading organization or adopting live runtimes. If a folder
is later deleted, creating or recovering a shell fails locally; it does not
silently fall back to another folder. Use Project folder to fix the setting.
An older workspace with no saved folder continues to use the launcher's project
directory, preserving the spike's previous behavior.

The JSON catalog stays version 1 with an optional workspace `directory` field.
The renderer projection uses catalog v4 with `D` records when any workspace has
a folder, and otherwise remains v3. New viewers still read v2/v3 and legacy
projections. Already-open older viewers keep their last valid organization when
encountering v4; reopen those windows after upgrading to see subsequent changes.
Workspace setting changes reuse terminal resources; no snapshot export, directory
checks or filesystem reads are added to pane rendering.

The service advertises supported methods through its health result. A new launcher
replaces an older organization service that lacks `workspace.create_at`, then
adopts the same live runtimes. This updates organization code, not terminal
executables or shells. It waits using readonly health probes and does not replay
sent mutations. Existing CLI `workspace.create` remains unchanged; the new
`workspace.create_at` method requires its `project_directory` argument so older
services cannot silently ignore the selected folder.

## Validation

`just check` includes real shells verifying legacy defaults, first/new/recovered
shell directories, unchanged live PID/cwd/socket identity, cross-workspace moves,
relative and Unicode/spaced paths, invalid input, missing-folder adoption/failure,
and old-service replacement without losing shell state. Catalog projection tests
verify folder refresh without reconnecting retained terminal resources.

`just multiwindow-gui-test` covers the project-folder editor, shared updates,
new-workspace name/folder input through Tab, first-shell working directory and
existing terminal ownership flows. Native macOS workspace
creation, capacity release, folder editing and new-terminal directory behavior were
confirmed by the owner. Native recovery-directory and invalid-path checks remain
pending. The owner also confirmed the selection and capacity cues on Linux and
macOS.

## Cursor editing acceptance

Open Rename workspace or Change project folder. Check that the existing value
remains visible with an unselected caret; click within it or use arrows to insert
and remove part of the value. Exercise Shift selection and drag selection, then
copy/cut/paste a selected span. Verify that Select all is needed for full-value
replacement and that Cancel leaves saved metadata and shell input untouched.
Check long paths at the beginning and end of the field, names/paths with spaces
and Unicode, rejected multi-line/oversized paste, and switching between both New
workspace fields with Tab. On Mac also check Command+C/X/V/A, Option+arrows,
Command+Left/Right and Option+Backspace. Native Mac validation is pending.

## Native folder browsing

New workspace and Change project folder now offer **Browse** next to the editable
path. The chosen folder fills the field and remains editable; Save/Create is still
explicit. Cancel or a picker error preserves the previous value and saved catalog.
The picker starts at the current folder, or the nearest existing parent of a
partially entered/missing path. Selecting a folder never sends `cd` to live shells.

The client runs a bounded-reply helper asynchronously. Dialog input stays modal
while terminals continue parsing/rendering. Escape in the parent can cancel the
picker, and window close terminates its private helper process group and detaches
terminals normally. Native cancellation leaves the workbench dialog open.
Folder controls participate in Tab/Shift+Tab: name, folder, Browse, Cancel, Create
for New workspace; folder, Browse, Cancel, Save for Change project folder.

Platform adapters are isolated under `scripts/platform_dialogs/`. Mac uses the
Standard Additions [`choose folder` API](https://developer.apple.com/library/archive/documentation/LanguagesUtilities/Conceptual/MacAutomationScriptingGuide/PromptforaFileorFolder.html)
via a fixed AppleScript file and separate path argument, with no Finder automation.
Linux uses [Zenity's directory selection](https://help.gnome.org/users/zenity/stable/file-selection.html.fr),
which can use the desktop portal. Zenity must be available where the viewer/helper
runs (inside the build container for container launches). If unavailable, an inline
error leaves manual entry working. No shell interpolation or new Python/Rust
dependency is involved. OS/native dialog styling follows the desktop.

`just folder-picker-gui-test` uses actual Zenity 4.2.2, an isolated Xvfb display,
private D-Bus session and temporary desktop preferences. Selection, cancellation,
keyboard Browse, explicit save/create, pending-picker close and sibling identity
pass on Linux. Existing text-edit/focus/multiwindow tests pass. `just check` covers
argument safety, platform cancellation/failure contracts, UTF-8/path bounds and
nearest-existing-folder behavior. The owner confirmed the Mac native picker on
2026-10-14; the remaining edge-case checks below still need explicit acceptance.

On Mac, open Change project folder, choose Browse, cancel once, then select a
folder. Confirm the value remains editable before Save and existing shells keep
their working directory. Repeat with New workspace; its first shell should start
in the chosen folder. Also close a disposable window while its picker is open.
