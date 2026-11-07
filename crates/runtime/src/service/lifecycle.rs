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

/// A private runtime-written file of at most 4 KiB.
fn read_evidence(socket: &Path, extension: &str) -> Option<Vec<u8>> {
    let mut file = OpenOptions::new()
        .read(true)
        .custom_flags(libc::O_NOFOLLOW | libc::O_NONBLOCK)
        .open(socket.with_extension(extension))
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
    (bytes.len() <= 4096).then_some(bytes)
}

/// The last screen lines an app left when it failed; text only, newline-separated.
fn read_output(socket: &Path) -> Option<String> {
    let text = String::from_utf8(read_evidence(socket, "output")?).ok()?;
    let clean = text.chars().all(|ch| ch == '\n' || !ch.is_control());
    (clean && !text.is_empty()).then_some(text)
}

fn read_record(socket: &Path) -> Option<Value> {
    let bytes = read_evidence(socket, "lifecycle")?;
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

/// Whether the runtime recorded that its shell or command ran and exited.
pub(crate) fn exited(socket: &Path) -> bool {
    read_record(socket).is_some_and(|value| value["event"] == "exited")
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
    let mut detail = json!({"reason": reason, "message": message, "record": value});
    if let Some(output) = (reason == "exited" || reason == "signal")
        .then(|| read_output(socket))
        .flatten()
    {
        detail["output"] = json!(output);
    }
    detail
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::os::unix::fs::PermissionsExt;

    #[test]
    fn missing_corrupt_or_unsafe_evidence_is_unknown() {
        let root = std::env::temp_dir().join(format!("wb-life-record-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let socket = root.join("one.sock");
        let path = socket.with_extension("lifecycle");
        let write = |value: &Value| {
            let _ = fs::remove_file(&path);
            fs::write(&path, value.to_string()).unwrap();
            fs::set_permissions(&path, fs::Permissions::from_mode(0o600)).unwrap();
        };
        let reason = |state: &str| detail(&socket, state)["reason"].clone();
        assert_eq!(reason("stopped"), "unknown");
        let record = json!({"version": 1, "runtime_pid": 123, "shell_pid": 456, "started_ms": 1,
            "event": "exited", "exit_code": 0, "signal": null});
        write(&record);
        assert_eq!(reason("stopped"), "exited");
        assert_eq!(
            reason("running"),
            "running",
            "an old receipt overrode a live endpoint"
        );
        assert_eq!(reason("unavailable"), "unavailable");
        let with = |key: &str, value: Value| {
            let mut changed = record.clone();
            changed[key] = value;
            changed
        };
        let mut without_signal = record.clone();
        without_signal.as_object_mut().unwrap().remove("signal");
        for bad in [
            with("version", json!(true)),
            with("exit_code", json!(true)),
            with("exit_code", json!(-1)),
            with("signal", json!("Killed")),
            with("event", json!("running")),
            with("event", json!("imaginary")),
            with("event", json!([])),
            with("signal", json!("bad\ntext")),
            json!({"version": 1, "runtime_pid": 123, "shell_pid": 456, "started_ms": 1,
                "event": "exited", "exit_code": null, "signal": null}),
            without_signal,
            json!([]),
            json!({"version": 2}),
        ] {
            write(&bad);
            assert!(read_record(&socket).is_none(), "accepted {bad}");
            assert_eq!(reason("stopped"), "unknown");
        }
        for text in ["x".repeat(4097), "{broken".to_owned()] {
            fs::write(&path, text).unwrap();
            assert!(read_record(&socket).is_none());
        }
        write(&record);
        fs::set_permissions(&path, fs::Permissions::from_mode(0o666)).unwrap();
        assert!(
            read_record(&socket).is_none(),
            "accepted a world-writable record"
        );
        fs::remove_file(&path).unwrap();
        let other = root.join("other");
        fs::write(&other, record.to_string()).unwrap();
        std::os::unix::fs::symlink(&other, &path).unwrap();
        assert!(read_record(&socket).is_none(), "followed a symlink");
        fs::remove_file(&path).unwrap();
        let fifo = std::ffi::CString::new(path.as_os_str().as_encoded_bytes()).unwrap();
        // SAFETY: a valid NUL-terminated path; mkfifo has no other preconditions.
        assert_eq!(unsafe { libc::mkfifo(fifo.as_ptr(), 0o600) }, 0);
        assert!(read_record(&socket).is_none(), "read from a FIFO");
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn failed_apps_report_their_last_output() {
        let root = std::env::temp_dir().join(format!("wb-life-output-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let socket = root.join("one.sock");
        let private = |extension: &str, text: &str| {
            let path = socket.with_extension(extension);
            fs::write(&path, text).unwrap();
            fs::set_permissions(&path, fs::Permissions::from_mode(0o600)).unwrap();
        };
        private(
            "lifecycle",
            &json!({"version": 1, "runtime_pid": 123, "shell_pid": 456,
            "started_ms": 1, "event": "exited", "exit_code": 127, "signal": null})
            .to_string(),
        );
        assert!(detail(&socket, "stopped").get("output").is_none());
        private("output", "zsh:1: command not found: htop\n");
        let found = detail(&socket, "stopped");
        assert_eq!(found["output"], "zsh:1: command not found: htop\n");
        assert_eq!(found["record"]["exit_code"], 127);
        assert!(detail(&socket, "running").get("output").is_none());
        for bad in ["\x1b[31mred\n", ""] {
            private("output", bad);
            assert!(
                detail(&socket, "stopped").get("output").is_none(),
                "accepted {bad:?}"
            );
        }
        private("output", &"x".repeat(4097));
        assert!(detail(&socket, "stopped").get("output").is_none());
        fs::remove_dir_all(root).unwrap();
    }
}
