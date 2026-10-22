//! Agent launchers and lifecycle hook observers. Hooks report state to the
//! runtime control socket; they never produce hook decisions or prompt output.
use serde_json::{json, Map, Value};
use std::io::{Read, Write};
use std::os::unix::net::UnixStream;
use std::os::unix::process::CommandExt;
use std::path::{Path, PathBuf};
use std::time::Duration;

const LIMIT: u64 = 1024 * 1024;
pub const AGENTS: [&str; 4] = ["codex", "claude", "copilot", "pi"];
const COMMON: [&str; 7] = [
    "SessionStart",
    "SessionEnd",
    "UserPromptSubmit",
    "PreToolUse",
    "PermissionRequest",
    "PostToolUse",
    "Stop",
];
pub const COPILOT_EVENTS: [(&str, &str); 9] = [
    ("sessionStart", "SessionStart"),
    ("sessionEnd", "SessionEnd"),
    ("userPromptSubmitted", "UserPromptSubmit"),
    ("preToolUse", "PreToolUse"),
    ("postToolUse", "PostToolUse"),
    ("postToolUseFailure", "PostToolUseFailure"),
    ("agentStop", "Stop"),
    ("errorOccurred", "ErrorOccurred"),
    ("notification", "Notification"),
];

fn truthy(event: &Value, field: &str) -> bool {
    match event.get(field) {
        None | Some(Value::Null) | Some(Value::Bool(false)) => false,
        Some(Value::String(text)) => !text.is_empty(),
        Some(Value::Number(number)) => number.as_f64() != Some(0.0),
        Some(Value::Array(list)) => !list.is_empty(),
        Some(Value::Object(object)) => !object.is_empty(),
        Some(Value::Bool(true)) => true,
    }
}

/// Status tags: 0 clears, 1 working, 2 needs input, 3 ready.
fn hook_state(event: &Value) -> Option<u8> {
    if !event.is_object() || truthy(event, "agent_id") || truthy(event, "agent_type") {
        return None; // A child must not overwrite the foreground parent's state.
    }
    match event["hook_event_name"].as_str()? {
        "SessionStart" | "Stop" | "Interrupt" => Some(3),
        "SessionEnd" => Some(0),
        "PermissionRequest" | "Elicitation" => Some(2),
        "PreToolUse" => Some(
            if matches!(
                event["tool_name"].as_str(),
                Some("AskUserQuestion" | "request_user_input")
            ) {
                2
            } else {
                1
            },
        ),
        "UserPromptSubmit" | "PostToolUse" | "PostToolUseFailure" | "ElicitationResult" => Some(1),
        "Notification" => match event["notification_type"].as_str() {
            Some("permission_prompt" | "elicitation_dialog") => Some(2),
            Some("idle_prompt") => Some(3),
            _ => None,
        },
        _ => None,
    }
}

fn copilot_state(event: &Value, configured: &str) -> Option<u8> {
    let name = COPILOT_EVENTS
        .iter()
        .find(|(event, _)| *event == configured)?
        .1;
    if !event.is_object() {
        return None;
    }
    let children = [
        "agent_id",
        "agent_type",
        "agentId",
        "agentType",
        "agentName",
        "parentSessionId",
    ];
    if children.iter().any(|field| truthy(event, field)) {
        return None;
    }
    match event.get("hook_event_name") {
        None | Some(Value::Null) => {}
        Some(supplied) if supplied == name || supplied == configured => {}
        Some(_) => return None,
    }
    // PermissionRequest is before the rules engine, not proof of a prompt.
    // Only actual notifications or the question tool establish Needs input.
    match name {
        "ErrorOccurred" => Some(0),
        "Notification" => matches!(
            event["notification_type"].as_str(),
            Some("permission_prompt" | "elicitation_dialog")
        )
        .then_some(2),
        "PreToolUse" => {
            let tool = event.get("toolName").or_else(|| event.get("tool_name"));
            Some(
                if matches!(
                    tool.and_then(Value::as_str),
                    Some("ask_user" | "AskUserQuestion")
                ) {
                    2
                } else {
                    1
                },
            )
        }
        _ => {
            let mut renamed = event.clone();
            renamed["hook_event_name"] = json!(name);
            hook_state(&renamed)
        }
    }
}

fn monotonic_ns() -> u64 {
    let mut time = libc::timespec {
        tv_sec: 0,
        tv_nsec: 0,
    };
    // SAFETY: time is a valid timespec; CLOCK_MONOTONIC is always available.
    unsafe { libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut time) };
    time.tv_sec as u64 * 1_000_000_000 + time.tv_nsec as u64
}

fn decode_hex(text: &str) -> Option<Vec<u8>> {
    if !text.len().is_multiple_of(2) {
        return None;
    }
    (0..text.len())
        .step_by(2)
        .map(|index| u8::from_str_radix(text.get(index..index + 2)?, 16).ok())
        .collect()
}

fn report_status(agent: &str, configured: Option<&str>) -> Option<()> {
    // Same clock as the runtime's stale-report check across hook processes.
    let sequence = monotonic_ns();
    let tag: u8 = match agent {
        "codex" => 1,
        "claude" => 2,
        "copilot" => 6,
        _ => return None,
    };
    let mut raw = Vec::new();
    std::io::stdin()
        .lock()
        .take(LIMIT + 1)
        .read_to_end(&mut raw)
        .ok()?;
    if raw.len() as u64 > LIMIT {
        return None;
    }
    let event: Value = serde_json::from_slice(&raw).ok()?;
    let status = if agent == "copilot" {
        copilot_state(&event, configured?)
    } else {
        hook_state(&event)
    }?;
    let socket = PathBuf::from(std::env::var_os("WORKBENCH_SESSION_SOCKET")?);
    let group: i32 = std::env::var("WORKBENCH_AGENT_GROUP").ok()?.parse().ok()?;
    let token = decode_hex(&std::env::var("WORKBENCH_AGENT_RUN").ok()?)?;
    if token.len() != 16 || group <= 0 {
        return None;
    }
    let mut packet = b"STATE".to_vec();
    packet.extend([tag, status]);
    packet.extend(group.to_be_bytes());
    packet.extend(&token);
    packet.extend(sequence.to_be_bytes());
    let mut connection = UnixStream::connect(socket.with_extension("ctl")).ok()?;
    connection
        .set_read_timeout(Some(Duration::from_millis(400)))
        .ok()?;
    connection
        .set_write_timeout(Some(Duration::from_millis(400)))
        .ok()?;
    connection.write_all(&packet).ok()?;
    let mut reply = [0; 16];
    let _ = connection.read(&mut reply);
    Some(())
}

/// Observe one hook event. Unavailable reporting must not block or alter the agent.
pub fn report(agent: &str, configured: Option<&str>) {
    let _ = report_status(agent, configured);
}

fn shell_quote(text: &str) -> String {
    let safe = !text.is_empty()
        && text
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || b"@%+=:,./-_".contains(&byte));
    if safe {
        text.to_owned()
    } else {
        format!("'{}'", text.replace('\'', "'\"'\"'"))
    }
}

fn integrations() -> Option<PathBuf> {
    let bin = PathBuf::from(std::env::var_os("WORKBENCH_AGENT_BIN")?);
    Some(bin.parent()?.join("integrations"))
}

fn arguments(agent: &str, tail: Vec<String>, executable: &Path) -> Result<Vec<String>, String> {
    let directory = integrations();
    if agent == "pi" {
        let extension = directory.map(|path| path.join("pi/workbench.ts"));
        let Some(extension) = extension.filter(|path| path.is_file()) else {
            return Err("Pi observers are missing; rebuild or install a current preview.".into());
        };
        let mut args = vec![
            "pi".into(),
            "--extension".into(),
            extension.to_string_lossy().into_owned(),
        ];
        args.extend(tail);
        return Ok(args);
    }
    if agent == "copilot" {
        let plugin = directory.map(|path| path.join("copilot"));
        let Some(plugin) = plugin.filter(|path| path.join("hooks.json").is_file()) else {
            return Err("Copilot observers are missing; rebuild with just spike.".into());
        };
        let mut args = vec![
            "copilot".into(),
            "--plugin-dir".into(),
            plugin.to_string_lossy().into_owned(),
        ];
        args.extend(tail);
        return Ok(args);
    }
    let command = format!(
        "{} agent-hook {agent}",
        shell_quote(&executable.to_string_lossy())
    );
    let extra: &[&str] = if agent == "codex" {
        &["Interrupt"]
    } else {
        &[
            "PostToolUseFailure",
            "Notification",
            "Elicitation",
            "ElicitationResult",
        ]
    };
    let events: Vec<&str> = COMMON.iter().chain(extra).copied().collect();
    let mut args = if agent == "claude" {
        let hooks: Map<String, Value> = events
            .iter()
            .map(|event| {
                let entry =
                    json!([{"hooks": [{"type": "command", "command": command, "timeout": 2}]}]);
                ((*event).to_owned(), entry)
            })
            .collect();
        vec![
            "claude".into(),
            "--settings".into(),
            json!({"hooks": hooks}).to_string(),
        ]
    } else {
        // Inline TOML in a session layer is additive to hooks from other layers.
        let mut args = vec!["codex".to_owned(), "--no-daemon".to_owned()];
        let quoted = Value::String(command).to_string();
        for event in events {
            args.push("-c".into());
            args.push(format!(
                "hooks.{event}=[{{hooks=[{{type=\"command\",command={quoted},timeout=2}}]}}]"
            ));
        }
        args
    };
    args.extend(tail);
    Ok(args)
}

/// Replace this process with the agent, with invocation-local observers.
/// Returns only on failure, with the message to report.
pub fn launch(agent: &str, tail: Vec<String>) -> String {
    if !AGENTS.contains(&agent) {
        return "Usage: wb-codex|wb-claude|wb-copilot|wb-pi [agent arguments...]".into();
    }
    if std::env::var_os("WORKBENCH_SESSION_SOCKET").is_none_or(|value| value.is_empty()) {
        return "Run wb-codex, wb-claude, wb-copilot or wb-pi inside a newly created Workbench terminal.".into();
    }
    let executable = match std::env::current_exe() {
        Ok(path) => path,
        Err(error) => return format!("Could not locate the Workbench runtime: {error}"),
    };
    let args = match arguments(agent, tail, &executable) {
        Ok(args) => args,
        Err(message) => return message,
    };
    let run = match std::fs::File::open("/dev/urandom").and_then(|mut file| {
        let mut bytes = [0; 16];
        file.read_exact(&mut bytes).map(|_| bytes)
    }) {
        Ok(bytes) => bytes
            .iter()
            .map(|byte| format!("{byte:02x}"))
            .collect::<String>(),
        Err(error) => return format!("Could not create an agent run token: {error}"),
    };
    // SAFETY: getpgrp has no preconditions and cannot fail.
    let group = unsafe { libc::getpgrp() };
    let error = std::process::Command::new(&args[0])
        .args(&args[1..])
        .env("WORKBENCH_AGENT_GROUP", group.to_string())
        .env("WORKBENCH_AGENT_RUN", run)
        .env("WORKBENCH_AGENT_PID", std::process::id().to_string())
        .exec();
    format!("Could not launch {}: {error}", args[0])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hook_events_map_to_status_and_ignore_children() {
        let state = |event: Value| hook_state(&event);
        assert_eq!(
            state(json!({"hook_event_name": "UserPromptSubmit"})),
            Some(1)
        );
        assert_eq!(
            state(json!({"hook_event_name": "PermissionRequest"})),
            Some(2)
        );
        assert_eq!(
            state(json!({"hook_event_name": "PreToolUse", "tool_name": "request_user_input"})),
            Some(2)
        );
        assert_eq!(
            state(json!({"hook_event_name": "PreToolUse", "tool_name": "Bash"})),
            Some(1)
        );
        assert_eq!(
            state(json!({"hook_event_name": "Notification", "notification_type": "idle_prompt"})),
            Some(3)
        );
        assert_eq!(state(json!({"hook_event_name": "Stop"})), Some(3));
        assert_eq!(state(json!({"hook_event_name": "SessionEnd"})), Some(0));
        assert_eq!(
            state(json!({"hook_event_name": "Stop", "agent_id": "child"})),
            None
        );
        assert_eq!(
            state(json!({"hook_event_name": "Stop", "agent_id": ""})),
            Some(3)
        );
        assert_eq!(state(json!({"hook_event_name": "invented"})), None);
        assert_eq!(state(json!([])), None);
    }

    #[test]
    fn copilot_reports_actual_prompts_only() {
        let cases = [
            ("sessionStart", json!({}), Some(3)),
            ("userPromptSubmitted", json!({}), Some(1)),
            ("preToolUse", json!({"toolName": "bash"}), Some(1)),
            ("preToolUse", json!({"toolName": "ask_user"}), Some(2)),
            (
                "preToolUse",
                json!({"hook_event_name": "PreToolUse", "tool_name": "AskUserQuestion"}),
                Some(2),
            ),
            (
                "notification",
                json!({"notification_type": "permission_prompt"}),
                Some(2),
            ),
            (
                "notification",
                json!({"notification_type": "elicitation_dialog"}),
                Some(2),
            ),
            (
                "notification",
                json!({"notification_type": "agent_idle"}),
                None,
            ),
            (
                "notification",
                json!({"notification_type": "agent_completed"}),
                None,
            ),
            ("postToolUse", json!({}), Some(1)),
            ("postToolUseFailure", json!({}), Some(1)),
            ("agentStop", json!({}), Some(3)),
            ("sessionEnd", json!({}), Some(0)),
            ("errorOccurred", json!({}), Some(0)),
            ("permissionRequest", json!({}), None),
            ("subagentStop", json!({}), None),
            (
                "agentStop",
                json!({"hook_event_name": "SubagentStop"}),
                None,
            ),
        ];
        for (name, event, expected) in cases {
            assert_eq!(copilot_state(&event, name), expected, "{name} {event}");
            for field in [
                "agent_id",
                "agent_type",
                "agentId",
                "agentType",
                "agentName",
                "parentSessionId",
            ] {
                let mut child = event.clone();
                child[field] = json!("child");
                assert_eq!(copilot_state(&child, name), None);
            }
        }
        for event in [json!([]), Value::Null, json!("text")] {
            assert_eq!(copilot_state(&event, "agentStop"), None);
        }
    }

    #[test]
    fn launch_arguments_keep_user_arguments_last() {
        let tail = vec!["--resume".to_owned(), "a spaced argument".to_owned()];
        let executable = Path::new("/opt/work bench/workbench-session");
        let claude = arguments("claude", tail.clone(), executable).unwrap();
        assert_eq!(claude[..2], ["claude", "--settings"]);
        let settings: Value = serde_json::from_str(&claude[2]).unwrap();
        assert_eq!(
            settings["hooks"]["PermissionRequest"][0]["hooks"][0]["command"],
            "'/opt/work bench/workbench-session' agent-hook claude"
        );
        assert_eq!(claude[3..], tail);
        let codex = arguments("codex", tail.clone(), executable).unwrap();
        assert_eq!(codex[..2], ["codex", "--no-daemon"]);
        assert_eq!(codex.len(), 2 + 16 + 2);
        assert!(codex[3].starts_with(
            "hooks.SessionStart=[{hooks=[{type=\"command\",command=\"'/opt/work bench/"
        ));
        assert_eq!(codex[codex.len() - 2..], tail);
        assert_eq!(shell_quote("it's"), "'it'\"'\"'s'");
    }
}
