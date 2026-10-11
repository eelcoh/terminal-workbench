# Experimental session transport

This is a local Unix-socket contract for a single terminal spike, not a published
API. The socket is mode 0600 in a caller-supplied private directory. Bind fails
if the socket path already exists; never unlink a running server automatically.

Each frame consists of a kind byte, a four-byte big-endian payload length, then
the payload. Payloads are limited to 65,536 bytes.

| Direction | Kind | Payload |
| --- | --- | --- |
| Server to viewer | 1 | Raw PTY output bytes |
| Server to viewer | 2 | Columns then rows, two big-endian u16 values |
| Server to viewer | 3 | Empty: historical replay finished, live input may begin |
| Server to viewer | 4 | UTF-8 attachment rejection |
| Viewer to server | 16 | Raw terminal input bytes |
| Viewer to server | 17 | Columns then rows, two big-endian u16 values |

Resize requests must be nonzero and at most 1000 rows/columns. The server updates
the PTY before broadcasting a resize frame. History starts at 80×24 and retains
output and resize events in one order so reattachment preserves reflow and
alternate-screen transitions. During replay, viewers suppress terminal-generated
responses to avoid injecting historical query replies into the current shell.

The event queue, live viewer queue, input queue, and history are bounded. Slow or
malformed viewers lose their connection, not the session. Only the currently
attached connection can write or resize. The shell lifetime follows the session
server, not a viewer. A shell exit ends the server and removes its socket.

## Explicit stop

Each new runtime also binds a mode-0600 Unix control socket obtained by replacing
the session socket's extension with `.ctl`. The neutral CLI is
`workbench-session stop SOCKET`. It sends exactly `STOP\n`; the server responds
`OK\n` after terminating/reaping the shell and removing both socket paths.
Malformed requests are discarded and reads time out after one second. This control
endpoint is independent of viewer attachment and terminal replay. Stop kills the
current foreground process group and terminates the shell; it does not enumerate
detached descendants. Repeated stop after socket removal succeeds. Earlier servers
without the control endpoint are not automatically upgraded or killed.

## Optional agent identification

The original terminal byte stream is unchanged. Agent metadata travels on a
separate connection to the private .ctl endpoint. `INFO\n` returns one bounded
JSON line and disconnects; `WATCH` (exactly five bytes, no newline) subscribes.
The watch uses the same five-byte frame header, with kind 5 and a single-byte
identity: 0 none recognized, 1 Codex, 2 Claude, 3 Gemini, 4 OpenCode, 5 Aider,
255 unavailable. Clients treat other tags as unavailable. The server sends a
current snapshot, then only changes. Updates are not appended to PTY replay.
The runtime allows eight watchers, each with an eight-frame queue; slow/invalid
watchers lose only their metadata connection. Closing a watcher releases it.
Watch reads reject input; commands still have the one-second initial deadline.

Older control endpoints reject WATCH/INFO without affecting the terminal stream.
Older viewers never subscribe and continue receiving only their existing terminal
frames. Agent identity is a once-per-second foreground-process heuristic, not
working/waiting status. See [identification limits](agent-identification.md).


## Optional lifecycle reports

`WAT2\n` subscribes to kind 6 frames with two bytes: the existing identity tag
and a status tag (0 unknown, 1 working, 2 waiting, 3 ready). A current snapshot
is followed by changes. WATCH/kind 5 remains unchanged; status-only updates do
not enter legacy subscriptions or terminal replay. The viewer tries WAT2 then
WATCH once if an older control server rejects it. A lost subscription clears
its evidence to Unknown without disconnecting the terminal.

`STATE` followed by 30 bytes reports agent tag (u8), status tag (u8), foreground
process group (big endian i32), invocation token (16 nonzero bytes), and sequence
(big endian u64). The runtime refreshes identity/group before validation. Only a
matching recognized agent/group, the bound token, and a newer nonzero sequence
are accepted. First report binds its token; identity/group changes clear both
status and token. Response is OK\n or ERROR\n. These private same-user reports
are observer hints, not authenticated process identity or a permission boundary.
INFO adds optional status and status_source fields; existing identity fields keep
their meanings. No terminal content, prompts, or tool input enter the report.

## Runtime terminal state

New control endpoints accept `TEXT\n` and `SNAP\n` (exactly five bytes). Each
returns one frame: a status byte (1 success, 0 failure), a big-endian u32 byte
length and the payload, then closes. TEXT returns plain active-screen/history
bytes; SNAP returns the pinned Ghostty binary snapshot. These named methods use
a separate 64 MiB response limit; the original terminal stream keeps its 65,536
byte frame limit. No existing command or frame changes meaning.

The VT is owned by the session event loop and reads output while detached.
The runtime answers queries only without an attached viewer; legacy attached
viewers keep their existing reply ownership. Reads do not claim viewer attachment
or resize the PTY. Older control servers discard these unknown methods; the CLI
reports endpoint unavailability without replacing their sessions. New viewers negotiate snapshot attachment as described below. See [runtime hardening](runtime-hardening.md).


## Snapshot attachment (ATS1)

On the control socket, send exactly `ATS1\n` followed by the 40 ASCII bytes of
VT revision `f64f4aca2c29b554d111b36c3d946a9bddd159ff`. This method switches the
same stream to a persistent single-viewer terminal connection. Incompatible
revisions, occupied sessions and export errors return kind 4 and close without
claiming attachment or restarting the runtime. Old servers close unknown methods;
a viewer may then reconnect through the legacy session socket.

The initial server sequence is:

| Kind | Payload |
| --- | --- |
| 7 | Four-byte big-endian complete snapshot length, nonzero and at most 64 MiB |
| 8 | Snapshot bytes, in nonempty chunks of at most 65,536 bytes |
| 9 | Empty snapshot finish marker |
| 3 | Empty READY; live input and resize may begin |

Snapshot capture and client registration occur on the same runtime event loop.
Live kind 1/2 frames queued after capture follow READY on the writer thread.
The client validates the exact declared length, complete Ghostty snapshot and
pinned continuation limit before binding its local effects and colors. Snapshot
geometry comes from the restored VT; desired window geometry is requested only
after READY. The viewer owns live query replies just as it does with legacy
attachment; restoration itself does not feed historical escape bytes or emit
query replies. Input kinds 16/17 retain their existing meaning and bounds.

Legacy attachment and its 16 MiB journal remain available and unchanged. Journal
overflow rejects only legacy attachments; ATS1 can still capture current state.
The current complete-snapshot implementation finishes all history before READY;
incremental history decoding is future work. Snapshot bytes are experimental and
are negotiated by exact VT revision, without a cross-version format promise.

## Explicit ownership transfer (ATM1)

`ATM1\n` followed by the same 40-byte VT revision explicitly requests ownership
of a live terminal. Ordinary ATS1 attachments still reject an occupied terminal.
After validating the revision and capturing a snapshot, the runtime registers
the replacement connection and queues an empty kind 10 frame to the previous
owner. Its writer drains bounded queued output, sends the revocation and closes.
Only the new connection ID may write or resize; delayed input, resize and detach
from the previous connection are ignored. Snapshot failures leave the old owner
in place. The shell, terminal state and socket identity are preserved.

A viewer receiving kind 10 shows “Open in another window” and offers “Move here”
in its terminal view. Claiming ownership is an explicit click, never an automatic
retry or consequence of navigating. Snapshots retain the limitations described
above, including unavailable retained graphics. These private same-user methods
are not an authentication boundary.

Older runtimes close unknown ATM1 requests. Before any response, the viewer retries
ordinary ATS1 without revoking its owner. If occupied, it explains that the owning
window must be closed first; clicking Reconnect afterward reconnects to the same
shell. Partial snapshots and incompatible revisions never trigger this fallback.
Missing older snapshot methods may still use ordinary legacy replay attachment.
Rebuilding does not replace an already-running runtime.

## Organization service and runtime capabilities

Window launch and management commands now route through a separate private
`service.rpc` endpoint under the state directory. It uses one bounded JSON line
per request/reply, protocol 1, serialized catalog operations and explicit health,
shutdown and selected recovery methods. It does not carry PTY output or input.
See [session service](session-service.md) for lifecycle, safety and validation.

New runtime control sockets also accept `CAPS\n` (five bytes), returning one JSON
line with protocol 1, runtime PID and `snapshot`, `transfer`, `terminal-read`
capabilities. This read-only query leaves attachment ownership unchanged.
Missing capability replies are reported as unadvertised rather than assuming an
older runtime supports or lacks a particular attachment method. Existing methods
and frame formats are unchanged; existing runtimes are never replaced implicitly.
