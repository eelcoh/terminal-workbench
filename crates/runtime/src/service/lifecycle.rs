//! Read bounded runtime evidence. Missing/corrupt evidence never blocks recovery.
use super::catalog::uid;
use serde_json::{json, Value};
use std::fs::OpenOptions;
use std::io::Read;
use std::os::unix::fs::{MetadataExt, OpenOptionsExt};
use std::path::Path;

const EVENTS: [&str; 7] = [
    "launching",
    "starting",
    "running",
    "exited",
    "stopped",
    "runtime_error",
    "startup_failed",
];

fn read_record(socket: &Path) -> Option<Value> {
    let mut file = OpenOptions::new()
        .read(true)
        .custom_flags(libc::O_NOFOLLOW | libc::O_NONBLOCK)
        .open(socket.with_extension("lifecycle"))
        .ok()?;
    let metadata = file.metadata().ok()?;
    if !metadata.is_file()
        || metadata.uid() != uid()
        || metadata.mode() & 0o022 != 0
        || metadata.len() > 4096
    {
        return None;
    }
    let mut bytes = Vec::new();
    file.by_ref().take(4097).read_to_end(&mut bytes).ok()?;
    let value: Value = serde_json::from_slice(&bytes).ok()?;
    let object = value.as_object()?;
    let required = [
        "version",
        "event",
        "runtime_pid",
        "shell_pid",
        "started_ms",
        "exit_code",
        "signal",
    ];
    if !required.iter().all(|key| object.contains_key(*key)) {
        return None;
    }
    let integer = |key: &str| value[key].as_i64().filter(|_| value[key].is_number());
    if integer("version") != Some(1) {
        return None;
    }
    let event = value["event"]
        .as_str()
        .filter(|event| EVENTS.contains(event))?;
    if !integer("started_ms").is_some_and(|started| started >= 1) {
        return None;
    }
    if event == "launching" {
        if !value["runtime_pid"].is_null() {
            return None;
        }
    } else if !integer("runtime_pid").is_some_and(|pid| pid >= 1) {
        return None;
    }
    if !value["shell_pid"].is_null() && !integer("shell_pid").is_some_and(|pid| pid >= 1) {
        return None;
    }
    let code = &value["exit_code"];
    let signal = &value["signal"];
    if !code.is_null()
        && !integer("exit_code").is_some_and(|code| (0..=0xffff_ffff).contains(&code))
    {
        return None;
    }
    if !signal.is_null() {
        let text = signal.as_str()?;
        if text.is_empty()
            || text.chars().count() > 80
            || text.chars().any(|ch| (ch as u32) < 32 || ch as u32 == 127)
        {
            return None;
        }
    }
    if !code.is_null() && !signal.is_null() {
        return None;
    }
    if matches!(event, "exited" | "stopped") && code.is_null() && signal.is_null() {
        return None;
    }
    if matches!(event, "launching" | "starting" | "running")
        && (!code.is_null() || !signal.is_null())
    {
        return None;
    }
    Some(value)
}

pub(crate) fn detail(socket: &Path, state: &str) -> Value {
    if state == "running" {
        return json!({"reason": "running", "message": "Shell is running."});
    }
    if state != "stopped" {
        return json!({"reason": "unavailable", "message": "Runtime status unavailable. Check again."});
    }
    let Some(value) = read_record(socket) else {
        return json!({"reason": "unknown", "message": "Runtime stopped; no exit reason was recorded."});
    };
    let (reason, message) = match value["event"].as_str().unwrap_or("") {
        "launching" => (
            "unknown",
            "Runtime stopped; no exit reason was recorded for this launch.".to_owned(),
        ),
        "stopped" => ("stopped", "Stopped by Workbench.".to_owned()),
        "startup_failed" => ("startup_failed", "Shell startup failed.".to_owned()),
        "starting" => (
            "startup_incomplete",
            "Runtime disappeared; shell startup was not recorded.".to_owned(),
        ),
        "running" => (
            "missing_exit",
            "Runtime disappeared; no shell exit was recorded.".to_owned(),
        ),
        "runtime_error" => (
            "runtime_error",
            "Runtime failed while handling the terminal.".to_owned(),
        ),
        _ => match (value["signal"].as_str(), value["exit_code"].as_i64()) {
            (Some(signal), _) => ("signal", format!("Shell ended by signal: {signal}.")),
            (None, Some(0)) => ("exited", "Shell exited normally (code 0).".to_owned()),
            (None, code) => (
                "exited",
                format!("Shell exited with code {}.", code.unwrap_or_default()),
            ),
        },
    };
    json!({"reason": reason, "message": message, "record": value})
}
