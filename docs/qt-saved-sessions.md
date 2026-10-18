# Qt saved-session launcher

Run `just toolkit-workbench` to open the Qt candidate on the existing saved
Workbench catalog. It uses the same `.local/sessions` directory as `just spike`;
`WORKBENCH_STATE_DIR` selects a different directory. An empty directory is seeded
by the existing session service. Existing catalogs are loaded as-is: this mode
does not add the study’s sample workspaces or rename your projects.

Closing the window detaches it. Workspaces and running shells remain in the
session service; reopening attaches to the same processes. A failed viewer also
leaves them intact. Stopped saved terminals remain stopped until you explicitly
choose **Start new shell**. **Close terminal…** still confirms before stopping
and removing that terminal. Initialization does not transfer ownership: a shell
owned by another window retains its owner until you explicitly choose **Move
here…**. A second saved Qt window uses the same catalog and service.

This is an opt-in saved-session slice, not the production frontend switch.
`just toolkit-ui` still creates disposable private sessions and stops them when
its window exits. Use it for experiments that should not affect real workspaces.

Current limits:

- Workspace organization and live processes persist. Qt window layout, focused
  panes, filters and workspace theme choices are still local to the window.
  Saved view profiles and workspace appearance are the next integration work.
- Font defaults and application shortcuts already use the separate user config
  directory. They do not move into the session catalog.
- For catalog changes made in another window, use **Refresh workspaces** in the
  application menu. Automatic catalog subscription is still pending in Qt.
- The existing service requires the same runtime path and project directory.
  Startup rejects a mismatched service rather than replacing it or its shells.
- Existing Unix socket length limits apply. If necessary, choose a shorter
  absolute `WORKBENCH_STATE_DIR` before creating a separate test session.

## Manual review

1. Run `just toolkit-workbench`. When another viewer owns a terminal, close that
   viewer and use Reconnect, or explicitly Move here.
2. In an available shell, set `wb_keep=still_here` and note `echo $$`.
3. Create a workspace or terminal, then close the Qt window normally.
4. Run `just toolkit-workbench` again. The workspace and terminal should remain;
   `echo "$wb_keep"` and `echo $$` should show the retained variable and PID.
5. Exit a shell normally, close and reopen the window. Its stopped entry should
   remain stopped until Start new shell is explicitly requested.

`just toolkit-saved-session-test` uses real PTYs and a temporary catalog to
verify retained organization, same shell/service PIDs across successful and
failed viewer exits, ephemeral launch-config cleanup and stopped-entry retention.
`just toolkit-ui-test` retains the X11/Wayland interaction and disposable-launcher
cleanup checks. Native Mac saved-window and migration acceptance remain pending.
