//! One-session Unix transport used only by the rendering spike.

use portable_pty::{native_pty_system, CommandBuilder, PtySize};
use std::ffi::OsString;
use std::fs;
use std::io::{self, Read, Write};
use std::net::Shutdown;
use std::os::unix::fs::PermissionsExt;
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::PathBuf;
use std::sync::mpsc::{self, SyncSender};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

const MAX_PAYLOAD: usize = 65_536;
const MAX_HISTORY: usize = 16 * 1024 * 1024;
const OUTPUT: u8 = 1;
const RESIZE: u8 = 2;
const READY: u8 = 3;
const ERROR: u8 = 4;
const INPUT: u8 = 16;
const REQUEST_RESIZE: u8 = 17;

type Frame = Arc<[u8]>;
type Failure = Box<dyn std::error::Error + Send + Sync>;

enum Event {
    Attach(u64, UnixStream),
    Detach(u64),
    Input(u64, Vec<u8>),
    Resize(u64, u16, u16),
    Output(Vec<u8>),
    End,
    Stop(UnixStream),
    AgentTick,
    Inspect(UnixStream),
    TerminalRead(UnixStream, bool),
    Watch(u64, UnixStream, bool),
    Signal(UnixStream, [u8; 30]),
    Unwatch(u64),
}

fn frame(kind: u8, payload: &[u8]) -> Frame {
    let mut bytes = Vec::with_capacity(5 + payload.len());
    bytes.push(kind);
    bytes.extend_from_slice(&(payload.len() as u32).to_be_bytes());
    bytes.extend_from_slice(payload);
    bytes.into()
}

fn size_frame(cols: u16, rows: u16) -> Frame {
    frame(RESIZE, &[cols.to_be_bytes(), rows.to_be_bytes()].concat())
}

fn read_request(stream: &mut impl Read) -> io::Result<(u8, Vec<u8>)> {
    let mut header = [0; 5];
    stream.read_exact(&mut header)?;
    let length = u32::from_be_bytes([header[1], header[2], header[3], header[4]]) as usize;
    if length > MAX_PAYLOAD
        || !matches!(header[0], INPUT | REQUEST_RESIZE)
        || (header[0] == REQUEST_RESIZE && length != 4)
    {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "invalid session frame",
        ));
    }
    let mut payload = vec![0; length];
    stream.read_exact(&mut payload)?;
    Ok((header[0], payload))
}

struct SocketFile(PathBuf);

impl Drop for SocketFile {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.0);
    }
}

fn start_client(
    id: u64,
    stream: UnixStream,
    history: Vec<Frame>,
    events: SyncSender<Event>,
) -> io::Result<SyncSender<Frame>> {
    stream.set_write_timeout(Some(Duration::from_secs(2)))?;
    let mut reader = stream.try_clone()?;
    let mut writer = stream;
    let (frames_tx, frames_rx) = mpsc::sync_channel::<Frame>(256);
    let input_events = events.clone();
    thread::spawn(move || {
        while let Ok((kind, payload)) = read_request(&mut reader) {
            let event = if kind == INPUT {
                Event::Input(id, payload)
            } else {
                let cols = u16::from_be_bytes([payload[0], payload[1]]);
                let rows = u16::from_be_bytes([payload[2], payload[3]]);
                if cols == 0 || rows == 0 || cols > 1000 || rows > 1000 {
                    break;
                }
                Event::Resize(id, cols, rows)
            };
            if input_events.send(event).is_err() {
                break;
            }
        }
        let _ = reader.shutdown(Shutdown::Both);
        let _ = input_events.send(Event::Detach(id));
    });
    thread::spawn(move || {
        let result = (|| -> io::Result<()> {
            for packet in history {
                writer.write_all(&packet)?;
            }
            writer.write_all(&frame(READY, &[]))?;
            for packet in frames_rx {
                writer.write_all(&packet)?;
            }
            Ok(())
        })();
        if let Err(error) = result {
            eprintln!("client {id} disconnected: {error}");
        }
        let _ = writer.shutdown(Shutdown::Both);
        let _ = events.send(Event::Detach(id));
    });
    Ok(frames_tx)
}

type AgentWatchers = Vec<(u64, SyncSender<Frame>, bool)>;
fn attention_frame(attention: &crate::attention::Attention, version2: bool) -> Frame {
    if version2 {
        frame(6, &[attention.agent.tag(), attention.status.tag()])
    } else {
        frame(5, &[attention.agent.tag()])
    }
}
fn publish_attention(
    watchers: &mut AgentWatchers,
    attention: &crate::attention::Attention,
    identity_changed: bool,
) {
    watchers.retain(|(_, frames, version2)| {
        if !version2 && !identity_changed {
            return true;
        }
        frames
            .try_send(attention_frame(attention, *version2))
            .is_ok()
    });
}

fn start_agent_watch(
    id: u64,
    stream: UnixStream,
    events: SyncSender<Event>,
    snapshot: Frame,
) -> io::Result<SyncSender<Frame>> {
    stream.set_write_timeout(Some(Duration::from_secs(1)))?;
    // Accepted control requests have a deadline; a watch is a persistent subscription.
    stream.set_read_timeout(None)?;
    let mut reader = stream.try_clone()?;
    let mut writer = stream;
    let (tx, rx) = mpsc::sync_channel::<Frame>(8);
    let reader_events = events.clone();
    thread::spawn(move || {
        let mut byte = [0];
        let _ = reader.read(&mut byte);
        let _ = reader.shutdown(Shutdown::Both);
        let _ = reader_events.send(Event::Unwatch(id));
    });
    thread::spawn(move || {
        let _ = (|| -> io::Result<()> {
            writer.write_all(&snapshot)?;
            for packet in rx {
                writer.write_all(&packet)?;
            }
            Ok(())
        })();
        let _ = writer.shutdown(Shutdown::Both);
        let _ = events.send(Event::Unwatch(id));
    });
    Ok(tx)
}

/// Run a foreground server. Closing its client never closes the PTY.
///
/// The caller supplies an existing private directory for the socket. An existing
/// socket is never removed automatically; binding it must fail visibly.
pub fn serve(socket: PathBuf, shell: OsString) -> Result<(), Failure> {
    let socket = std::path::absolute(socket)?;
    let listener = UnixListener::bind(&socket)?;
    let _socket_file = SocketFile(socket.clone());
    fs::set_permissions(&socket, fs::Permissions::from_mode(0o600))?;
    let control_path = socket.with_extension("ctl");
    let control = UnixListener::bind(&control_path)?;
    let control_file = SocketFile(control_path.clone());
    fs::set_permissions(&control_path, fs::Permissions::from_mode(0o600))?;
    let mut terminal = crate::terminal::Terminal::new()?;
    let pair = native_pty_system().openpty(PtySize {
        rows: 24,
        cols: 80,
        pixel_width: 0,
        pixel_height: 0,
    })?;
    let mut command = CommandBuilder::new(shell);
    command.cwd(std::env::current_dir()?);
    command.env("TERM", "xterm-256color");
    // A session is independent of whichever terminal or Herdr invoked it.
    command.env("TERM_PROGRAM", "terminal-workbench");
    command.env("WORKBENCH_SESSION_SOCKET", &socket);
    command.env_remove("WORKBENCH_AGENT_GROUP");
    command.env_remove("WORKBENCH_AGENT_RUN");
    if let Some(bin) = std::env::var_os("WORKBENCH_AGENT_BIN") {
        let mut paths = vec![std::path::PathBuf::from(bin)];
        if let Some(path) = std::env::var_os("PATH") {
            paths.extend(std::env::split_paths(&path));
        }
        command.env("PATH", std::env::join_paths(paths)?);
    }
    for (name, _) in std::env::vars_os() {
        if name.to_str().is_some_and(|name| name.starts_with("HERDR_")) {
            command.env_remove(name);
        }
    }
    let mut child = pair.slave.spawn_command(command)?;
    drop(pair.slave);
    let mut output = pair.master.try_clone_reader()?;
    let mut input = pair.master.take_writer()?;
    let (input_tx, input_rx) = mpsc::sync_channel::<Vec<u8>>(16);
    thread::spawn(move || {
        for bytes in input_rx {
            if let Err(error) = input.write_all(&bytes).and_then(|()| input.flush()) {
                eprintln!("PTY input ended: {error}");
                break;
            }
        }
    });
    let (events_tx, events_rx) = mpsc::sync_channel(128);
    let control_events = events_tx.clone();
    thread::spawn(move || {
        for (id, connection) in control.incoming().enumerate() {
            let Ok(mut stream) = connection else {
                continue;
            };
            let _ = stream.set_read_timeout(Some(Duration::from_secs(1)));
            let mut command = [0; 5];
            if stream.read_exact(&mut command).is_err() {
                continue;
            }
            let event = match &command {
                b"STOP\n" => Event::Stop(stream),
                b"INFO\n" => Event::Inspect(stream),
                b"TEXT\n" => Event::TerminalRead(stream, false),
                b"SNAP\n" => Event::TerminalRead(stream, true),
                b"WATCH" => Event::Watch(id as u64, stream, false),
                b"WAT2\n" => Event::Watch(id as u64, stream, true),
                b"STATE" => {
                    let mut report = [0; 30];
                    if stream.read_exact(&mut report).is_err() {
                        continue;
                    }
                    Event::Signal(stream, report)
                }
                _ => continue,
            };
            if control_events.send(event).is_err() {
                break;
            }
        }
    });
    let tick_events = events_tx.clone();
    thread::spawn(move || loop {
        thread::sleep(Duration::from_secs(1));
        match tick_events.try_send(Event::AgentTick) {
            Ok(()) | Err(mpsc::TrySendError::Full(_)) => {}
            Err(mpsc::TrySendError::Disconnected(_)) => break,
        }
    });
    let output_events = events_tx.clone();
    thread::spawn(move || {
        let mut buffer = [0; 4096];
        loop {
            match output.read(&mut buffer) {
                Ok(0) => break,
                Ok(count) => {
                    if output_events
                        .send(Event::Output(buffer[..count].to_vec()))
                        .is_err()
                    {
                        return;
                    }
                }
                Err(error) if error.kind() == io::ErrorKind::Interrupted => continue,
                Err(error) => {
                    eprintln!("PTY output ended: {error}");
                    break;
                }
            }
        }
        let _ = output_events.send(Event::End);
    });
    let accept_events = events_tx.clone();
    thread::spawn(move || {
        for (index, connection) in listener.incoming().enumerate() {
            match connection {
                Ok(stream) => {
                    if accept_events
                        .send(Event::Attach(index as u64, stream))
                        .is_err()
                    {
                        break;
                    }
                }
                Err(error) => eprintln!("accept: {error}"),
            }
        }
    });
    eprintln!(
        "session listening at {}; shell pid {:?}",
        socket.display(),
        child.process_id()
    );
    let mut client: Option<(u64, SyncSender<Frame>)> = None;
    let mut history = vec![size_frame(80, 24)];
    let mut history_bytes = history[0].len();
    let mut replay_available = true;
    let mut stop_reply = None;
    let mut attention = crate::attention::Attention::new();
    let mut watchers: AgentWatchers = Vec::new();
    let result = (|| -> Result<(), Failure> {
        for event in events_rx {
            let packet = match event {
                Event::Attach(id, mut stream) => {
                    if client.is_some() || !replay_available {
                        stream.set_write_timeout(Some(Duration::from_secs(2)))?;
                        let reason = if client.is_some() {
                            "a window is already attached"
                        } else {
                            "replay exceeds the spike's 16 MiB limit; restart the session"
                        };
                        let _ = stream.write_all(&frame(ERROR, reason.as_bytes()));
                        continue;
                    }
                    client = Some((
                        id,
                        start_client(id, stream, history.clone(), events_tx.clone())?,
                    ));
                    continue;
                }
                Event::Detach(id) => {
                    if client.as_ref().is_some_and(|(current, _)| *current == id) {
                        client = None;
                    }
                    continue;
                }
                Event::Input(id, bytes) => {
                    if client.as_ref().is_some_and(|(current, _)| *current == id)
                        && input_tx.try_send(bytes).is_err()
                    {
                        eprintln!("client {id} disconnected: PTY input queue unavailable");
                        client = None;
                    }
                    continue;
                }
                Event::Resize(id, cols, rows) => {
                    if !client.as_ref().is_some_and(|(current, _)| *current == id) {
                        continue;
                    }
                    terminal.resize(cols, rows)?;
                    pair.master.resize(PtySize {
                        rows,
                        cols,
                        pixel_width: 0,
                        pixel_height: 0,
                    })?;
                    size_frame(cols, rows)
                }
                Event::Output(bytes) => {
                    // Legacy attached viewers still own their query replies. A
                    // detached session answers from the same state it snapshots.
                    match terminal.feed(&bytes, client.is_none()) {
                        Ok(replies) if !replies.is_empty() => {
                            if input_tx.try_send(replies).is_err() {
                                eprintln!("detached terminal reply queue unavailable");
                            }
                        }
                        Ok(_) => {}
                        Err(error) => eprintln!("terminal reply limit: {error}"),
                    }
                    frame(OUTPUT, &bytes)
                }
                Event::AgentTick => {
                    let group = pair.master.process_group_leader();
                    let next = crate::agent::identify(crate::platform::identity(group));
                    let old = attention.agent;
                    if attention.sample(next, group) {
                        publish_attention(&mut watchers, &attention, old != next);
                    }
                    continue;
                }
                Event::Signal(mut stream, report) => {
                    let group = pair.master.process_group_leader();
                    let next = crate::agent::identify(crate::platform::identity(group));
                    let old = attention.agent;
                    if attention.sample(next, group) {
                        publish_attention(&mut watchers, &attention, old != next);
                    }
                    let reported_group =
                        i32::from_be_bytes([report[2], report[3], report[4], report[5]]);
                    let mut run = [0; 16];
                    run.copy_from_slice(&report[6..22]);
                    let mut sequence = [0; 8];
                    sequence.copy_from_slice(&report[22..30]);
                    let accepted =
                        crate::attention::Status::from_tag(report[1]).is_some_and(|status| {
                            attention.report(
                                report[0],
                                reported_group,
                                run,
                                u64::from_be_bytes(sequence),
                                status,
                            )
                        });
                    if accepted {
                        publish_attention(&mut watchers, &attention, false);
                    }
                    let _ = stream.set_write_timeout(Some(Duration::from_secs(1)));
                    let _ = stream.write_all(if accepted { b"OK\n" } else { b"ERROR\n" });
                    continue;
                }
                Event::TerminalRead(mut stream, binary) => {
                    let (status, bytes) = match terminal.export(binary) {
                        Ok(bytes) => (1, bytes),
                        Err(error) => (0, error.to_string().into_bytes()),
                    };
                    let _ = stream.set_write_timeout(Some(Duration::from_secs(1)));
                    let packet = frame(status, &bytes);
                    let _ = stream.write_all(&packet);
                    continue;
                }
                Event::Inspect(mut stream) => {
                    stream.set_write_timeout(Some(Duration::from_secs(1)))?;
                    let _ = stream.write_all(attention.json().as_bytes());
                    continue;
                }
                Event::Watch(id, stream, version2) => {
                    if watchers.len() < 8 {
                        if let Ok(frames) = start_agent_watch(
                            id,
                            stream,
                            events_tx.clone(),
                            attention_frame(&attention, version2),
                        ) {
                            watchers.push((id, frames, version2));
                        }
                    }
                    continue;
                }
                Event::Unwatch(id) => {
                    watchers.retain(|(current, _, _)| *current != id);
                    continue;
                }
                Event::End => break,
                Event::Stop(stream) => {
                    stop_reply = Some(stream);
                    crate::platform::stop_group(pair.master.process_group_leader())?;
                    child.kill()?;
                    break;
                }
            };
            if replay_available {
                if history_bytes + packet.len() <= MAX_HISTORY {
                    history_bytes += packet.len();
                    history.push(packet.clone());
                } else {
                    // Never replay a truncated escape stream as a coherent screen.
                    replay_available = false;
                    history.clear();
                    eprintln!("replay capacity reached; new attachments disabled for this spike");
                }
            }
            if let Some((_, frames)) = &client {
                if frames.try_send(packet).is_err() {
                    // A slow renderer must not stall output parsing or grow memory.
                    client = None;
                }
            }
        }
        Ok(())
    })();
    if result.is_err() {
        let _ = child.kill();
    }
    drop(client);
    let status = child.wait()?;
    eprintln!("shell exited: {status}");
    drop(pair.master);
    drop(control_file);
    drop(_socket_file);
    if let Some(mut reply) = stop_reply {
        reply.set_write_timeout(Some(Duration::from_secs(2)))?;
        reply.write_all(if result.is_ok() { b"OK\n" } else { b"ERROR\n" })?;
    }
    result
}

/// Read runtime-owned terminal text or a pinned Ghostty binary snapshot without attaching a viewer.
pub fn read_terminal(socket: PathBuf, binary: bool) -> Result<Vec<u8>, Failure> {
    let mut stream = UnixStream::connect(socket.with_extension("ctl"))?;
    stream.set_read_timeout(Some(Duration::from_secs(3)))?;
    stream.set_write_timeout(Some(Duration::from_secs(1)))?;
    stream.write_all(if binary { b"SNAP\n" } else { b"TEXT\n" })?;
    let mut header = [0; 5];
    stream.read_exact(&mut header).map_err(|error| {
        io::Error::new(error.kind(), format!("terminal state endpoint unavailable; this session may predate runtime-owned terminal state ({error})"))
    })?;
    let length = u32::from_be_bytes([header[1], header[2], header[3], header[4]]) as usize;
    if length > 64 * 1024 * 1024 || header[0] > 1 {
        return Err("invalid terminal state response".into());
    }
    let mut bytes = vec![0; length];
    stream.read_exact(&mut bytes)?;
    if header[0] == 0 {
        return Err(String::from_utf8_lossy(&bytes).into_owned().into());
    }
    Ok(bytes)
}

/// Read current runtime identification without attaching a terminal viewer.
pub fn inspect(socket: PathBuf) -> Result<String, Failure> {
    let mut stream = UnixStream::connect(socket.with_extension("ctl"))?;
    stream.set_read_timeout(Some(Duration::from_secs(3)))?;
    stream.set_write_timeout(Some(Duration::from_secs(1)))?;
    stream.write_all(b"INFO\n")?;
    let mut result = String::new();
    stream.take(1024).read_to_string(&mut result)?;
    if !result.starts_with("{\"agent\":") {
        return Err("session has no identification endpoint; relaunch its shell".into());
    }
    Ok(result)
}

/// Stop the shell and its current foreground job, even with a viewer attached.
pub fn stop(socket: PathBuf) -> Result<(), Failure> {
    if !socket.exists() {
        return Ok(());
    }
    let mut stream = UnixStream::connect(socket.with_extension("ctl")).map_err(|error| {
        io::Error::new(
            error.kind(),
            format!("session has no stop endpoint; exit its shell and reopen ({error})"),
        )
    })?;
    stream.set_read_timeout(Some(Duration::from_secs(10)))?;
    stream.set_write_timeout(Some(Duration::from_secs(2)))?;
    stream.write_all(b"STOP\n")?;
    let mut response = [0; 3];
    stream.read_exact(&mut response)?;
    if &response != b"OK\n" {
        return Err("session stop failed".into());
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn codec_preserves_binary_input_and_rejects_invalid_shapes() {
        let packet = frame(INPUT, &[0, 255, 27, 13]);
        assert_eq!(
            read_request(&mut &packet[..]).expect("valid input"),
            (INPUT, vec![0, 255, 27, 13])
        );
        for packet in [
            frame(99, &[]),
            frame(REQUEST_RESIZE, &[0]),
            frame(INPUT, &vec![0; MAX_PAYLOAD + 1]),
        ] {
            assert_eq!(
                read_request(&mut &packet[..])
                    .expect_err("invalid frame")
                    .kind(),
                io::ErrorKind::InvalidData
            );
        }
    }
}
