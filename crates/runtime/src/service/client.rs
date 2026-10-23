//! Client side of `service.rpc`: start the authority when absent, then send one
//! request. Only connection establishment is retried, never a sent request.
use super::catalog::{fail, lock_file, os, Result};
use super::{endpoint, prepare_directory, PROTOCOL};
use serde_json::{json, Value};
use std::fs::OpenOptions;
use std::io::{self, Read, Write};
use std::os::unix::net::UnixStream;
use std::os::unix::process::CommandExt;
use std::path::Path;
use std::process::{Child, Command, Stdio};
use std::thread;
use std::time::{Duration, Instant};

enum Failure {
    Unavailable,
    TimedOut,
    Other(String),
}

impl From<String> for Failure {
    fn from(message: String) -> Self {
        Self::Other(message)
    }
}

fn exchange(
    directory: &Path,
    message: &Value,
    timeout: Duration,
) -> std::result::Result<Value, Failure> {
    let deadline = Instant::now() + timeout;
    let mut connection = match UnixStream::connect(endpoint(directory)?) {
        Ok(connection) => connection,
        Err(error)
            if matches!(
                error.raw_os_error(),
                Some(libc::ENOENT) | Some(libc::ECONNREFUSED)
            ) =>
        {
            return Err(Failure::Unavailable)
        }
        Err(error) => return Err(Failure::Other(os(error))),
    };
    let mut payload = message.to_string().into_bytes();
    payload.push(b'\n');
    connection.set_write_timeout(Some(timeout)).map_err(os)?;
    connection.write_all(&payload).map_err(os)?;
    let mut data = Vec::new();
    let mut chunk = [0; 4096];
    while !data.contains(&b'\n') {
        let remaining = deadline.saturating_duration_since(Instant::now());
        if remaining.is_zero() {
            return Err(Failure::TimedOut);
        }
        connection.set_read_timeout(Some(remaining)).map_err(os)?;
        match connection.read(&mut chunk) {
            Ok(0) => {
                return Err(Failure::Other(
                    "Session service closed before replying".into(),
                ))
            }
            Ok(count) => data.extend_from_slice(&chunk[..count]),
            Err(error)
                if matches!(
                    error.kind(),
                    io::ErrorKind::WouldBlock | io::ErrorKind::TimedOut
                ) =>
            {
                return Err(Failure::TimedOut)
            }
            Err(error) => return Err(Failure::Other(os(error))),
        }
        if data.len() > super::LIMIT {
            return Err(Failure::Other(
                "Session service message exceeds size limit".into(),
            ));
        }
    }
    let line = data.split(|byte| *byte == b'\n').next().unwrap_or_default();
    let reply: Value = serde_json::from_slice(line).map_err(|error| error.to_string())?;
    if reply["protocol"].as_i64() != Some(PROTOCOL) {
        return Err(Failure::Other(
            "Incompatible session service; stop it explicitly before restarting".into(),
        ));
    }
    if reply["ok"] != json!(true) {
        let error = reply["error"]
            .as_str()
            .unwrap_or("Session service rejected the request");
        return Err(Failure::Other(error.to_owned()));
    }
    Ok(reply)
}

/// Reap only a process group spawned by this caller with a new session.
fn stop_owned_startup(child: &mut Child) {
    let group = child.id() as libc::pid_t;
    // SAFETY: the group leader is our unreaped child, so its id is not reused.
    unsafe { libc::killpg(group, libc::SIGTERM) };
    let deadline = Instant::now() + Duration::from_secs(1);
    while Instant::now() < deadline && matches!(child.try_wait(), Ok(None)) {
        thread::sleep(Duration::from_millis(20));
    }
    // The leader may exit before a descendant that ignores SIGTERM.
    // SAFETY: as above; the group id stays reserved until the leader is reaped.
    unsafe { libc::killpg(group, libc::SIGKILL) };
    let _ = child.wait();
}

fn wait_for_startup(directory: &Path, child: &mut Child) -> Result<()> {
    let deadline = Instant::now() + Duration::from_secs(5);
    let failed = || {
        format!(
            "Session service did not start; inspect {}",
            directory.join("service.log").display()
        )
    };
    let health = json!({"protocol": PROTOCOL, "method": "health"});
    loop {
        let remaining = deadline.saturating_duration_since(Instant::now());
        if remaining.is_zero() {
            stop_owned_startup(child);
            return Err(failed());
        }
        // Only readiness is retried. The request is sent after this returns.
        match exchange(directory, &health, remaining) {
            Ok(_) => return Ok(()),
            Err(Failure::Unavailable | Failure::TimedOut) => {
                if !matches!(child.try_wait(), Ok(None)) {
                    stop_owned_startup(child);
                    return Err(failed());
                }
                thread::sleep(Duration::from_millis(20));
            }
            Err(Failure::Other(error)) => {
                stop_owned_startup(child);
                return Err(error);
            }
        }
    }
}

fn send(directory: &Path, message: &Value) -> std::result::Result<Value, Failure> {
    exchange(directory, message, Duration::from_secs(60))
}

fn finish(result: std::result::Result<Value, Failure>) -> Result<Value> {
    match result {
        Ok(reply) => Ok(reply),
        Err(Failure::Unavailable) => fail("Session service unavailable"),
        Err(Failure::TimedOut) => fail("Session service reply deadline expired"),
        Err(Failure::Other(error)) => Err(error),
    }
}

/// Retire the running service so a different install can start its own. Shells
/// keep running and the next service adopts them. Only the instance observed
/// here is retired, never a replacement that started meanwhile.
pub fn retire(directory: &Path) -> Result<()> {
    let health = finish(send(
        directory,
        &json!({"protocol": PROTOCOL, "method": "health"}),
    ))?;
    let Some(instance) = health["service_instance"].as_str() else {
        return fail("Session service did not report its instance; stop it before opening Band");
    };
    finish(send(
        directory,
        &json!({"protocol": PROTOCOL, "method": "service.retire", "expected_instance": instance}),
    ))?;
    let deadline = Instant::now() + Duration::from_secs(10);
    loop {
        match exchange(
            directory,
            &json!({"protocol": PROTOCOL, "method": "health"}),
            Duration::from_secs(2),
        ) {
            Err(Failure::Unavailable) => return Ok(()),
            _ if Instant::now() >= deadline => {
                return fail("Previous session service did not stop")
            }
            _ => thread::sleep(Duration::from_millis(50)),
        }
    }
}

/// Open the saved organization for a window, starting the service if needed.
/// Initialization never starts stopped saved terminals or takes ownership.
pub fn initialize(directory: &Path, runtime: &Path, cwd: &Path, agent_bin: &Path) -> Result<Value> {
    let message = json!({"protocol": PROTOCOL, "method": "initialize", "identity": "",
        "title": "", "destination": "", "position": -1, "runtime": runtime.to_string_lossy(),
        "cwd": cwd.to_string_lossy(), "project_directory": null});
    match send(directory, &message) {
        Err(Failure::Unavailable) => {}
        other => return finish(other),
    }
    prepare_directory(directory)?;
    endpoint(directory)?;
    // Serialize concurrent first launches. The server holds its own lock for
    // its lifetime; competing direct server starts cannot replace its socket.
    let _lock = lock_file(&directory.join("service-start.lock"), false).map_err(os)?;
    match send(directory, &message) {
        Err(Failure::Unavailable) => {}
        other => return finish(other),
    }
    let log = OpenOptions::new()
        .append(true)
        .create(true)
        .open(directory.join("service.log"))
        .map_err(os)?;
    let mut command = Command::new(runtime);
    command
        .arg("service")
        .arg("--state-dir")
        .arg(directory)
        .arg("--runtime")
        .arg(runtime)
        .arg("--cwd")
        .arg(cwd)
        .arg("--agent-bin")
        .arg(agent_bin)
        .stdin(Stdio::null())
        .stdout(log.try_clone().map_err(os)?)
        .stderr(log);
    // SAFETY: setsid is async-signal-safe and runs in the forked child.
    unsafe {
        command.pre_exec(|| {
            if libc::setsid() < 0 {
                return Err(io::Error::last_os_error());
            }
            Ok(())
        });
    }
    let mut child = command.spawn().map_err(os)?;
    wait_for_startup(directory, &mut child)?;
    // The ready service is now the authority; it outlives this launcher.
    drop(child);
    finish(send(directory, &message))
}
