# Interrupted catalog operations

Creation now saves its intended workspace/terminal identities before starting a
runtime. The existing locked `workspaces.json` remains the authority; optional
`pending_create` contains the launch identities. No new service or dependency is
introduced. UI selection, focus and layout remain per-window presentation state.

Successful creation publishes the same JSON and TSV projections as before. If
the writer dies before all shells start, the next initialization or management
operation clears the intent and repairs the projections. Listening runtimes keep
their PID, terminal state and stable identity. Missing runtimes remain stopped;
**Start new shell** still requires explicit confirmation. Failed partial launches
retain their successful shells. A creation error can therefore leave a saved
stopped entry; reopen Workbench to see it before retrying the action.

Fresh seeding uses the same intent. Interrupted seeding is not treated as a new
session on the next launch. Existing saved catalogs never automatically restart
missing shells. Explicit bulk or single-terminal recovery remains available.
Losing a mutation reply still does not cause an automatic retry: inspect the
catalog before creating another workspace or terminal.

Close keeps its existing `pending_close` intent. Both intent types are validated
before mutation: identities must exist, must be unique, and a workspace-close
plan must include every terminal it removes. Conflicting or corrupt intent is
preserved and reported as an error without starting or stopping processes.
Interrupted closes finish before further mutations or explicit recovery. A close
failure retains the remaining plan for retry; successful already-stopped entries
are harmless to stop again. Closing a window still only detaches its terminals.

Atomic replacements flush file data and sync the parent directory where supported.
This slice verifies process loss, not storage failure or power-loss guarantees.
JSON/projection replacements are separate writes; initialization repairs stale
TSV/label projections from JSON. Intent is bounded by the existing eight-workspace,
eight-terminal limits. No journaling or filesystem work enters rendering, terminal
parsing or PTY byte handling.

Service health optionally advertises `catalog-journal-v1` in `features`. Opening
the rebuilt Workbench updates an older organization service using the existing
service-only shutdown/adoption flow. It leaves existing shell runtimes alive;
the terminal transport and legacy method response contracts are unchanged.

`just check` includes real process-kill tests before launch, after partial launch,
after JSON commit before projection, and before/partway through workspace stop.
Tests verify identity/projection repair, repeated initialization, explicit recovery,
same-PID live adoption, sibling shell variables/socket identity and appearance.
They also cover normal launch errors, failed intent writes and malformed intent.
All tests use disposable states and shells. The owner confirmed this automated
crash suite on native Mac on 2026-10-14; broader graphical review remains open.

For native review, create a workspace and another terminal, close a disposable
workspace, then reopen the app. Existing/stopped terminals should keep their state
and removed entries should stay removed. Destructive crash-boundary verification
is automated in temporary state; it is not necessary to kill your daily-use service.
Explicit organization-preserving terminal shutdown is now available; see
[session shutdown](session-shutdown.md). Subscriptions and installation lifecycle
remain open.
