# Workspace project folders

A workspace can save a project folder shared by all windows. In **New workspace**,
enter a name and a Project folder; Tab visits the fields, Cancel and Create; Shift+Tab moves backward. For an
existing workspace, use **Folder** beside Rename in the sidebar. Type or paste an
absolute path, `~/...`, or a path relative to the Workbench project directory.
The focused field has an outline; selected text uses the theme's selection colors.
Opening the folder editor selects its current path, and typing replaces that
selection. The dialog labels this behavior. This slice provides a path editor;
a native folder browser remains follow-up.

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
