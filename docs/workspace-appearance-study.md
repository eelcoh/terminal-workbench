# Workspace-first Appearance study

The owner finds the four-target Appearance picker confusing and the text-based
follow checkbox less polished than the rest of the app. The current flow opens
on Application colors, even when the intended task is choosing a project theme.

The [interactive study](prototypes/workspace-appearance.html) makes the main picker
choose one workspace theme. It uses sample terminals and in-memory state; it does
not change the native app or saved preferences.

Proposed normal flow:

- Open **Workspace theme…** for the active workspace. Name the workspace in the
  title, keep search and brightness filters, and show representative palette cards.
- Preview its terminals and chrome together. Keep the picker surface steady while
  browsing so its controls do not change contrast at every selection.
- **Apply to workspace** saves the choice; **Cancel** restores the exact prior
  workspace. No Application/Workspace/Terminal/Defaults tabs or follow checkbox.
- New terminals inherit the workspace theme. New workspaces take their starting
  theme from dotfiles. Global chrome/default preferences become an advanced
  configuration concern rather than another target in this dialog.

The owner previously wanted terminal colors to distinguish remote boxes. Retain
optional terminal exceptions in the pane menu, outside the main workspace picker.
An existing exception must remain visible and preserved; a quiet message can say
that one terminal uses its own theme. If the owner instead wants uniform colors
within each workspace, decide that explicitly before changing saved overrides.
The study shows the simpler no-exception case.

Native implementation review must cover workspace-target stability while the
dialog is open, independent themes across workspaces, preview/cancel/save errors,
opening from Overview or a pane menu, keyboard focus/search, dark/light surfaces,
empty workspaces, dotfile/UI precedence and existing terminal exceptions. Retain
existing palettes and scoped records during any migration; do not silently flatten
them. Chrome-follow defaults and advanced fixed-chrome preferences need an explicit
policy, since the owner previously appreciated both independent and aligned chrome.

This is a proposal for review. The native scope model and dotfiles format remain
unchanged until the interaction policy is agreed.
