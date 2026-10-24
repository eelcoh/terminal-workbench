//! Persistent workspace organization: validation and journaled mutations of
//! `workspaces.json`. A legacy `workspaces.tsv` is read once for migration.
use serde_json::{json, Map, Value};
use std::fs::{self, DirBuilder, File, OpenOptions};
use std::io::{self, Read, Write};
use std::os::unix::ffi::OsStrExt;
use std::os::unix::fs::{DirBuilderExt, FileTypeExt, MetadataExt, OpenOptionsExt};
use std::os::unix::io::AsRawFd;
use std::os::unix::net::UnixStream;
use std::os::unix::process::CommandExt;
use std::path::{Component, Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

pub(crate) const MAX_WORKSPACES: usize = 8;
pub(crate) const MAX_TERMINALS: usize = 8;

pub(crate) type Result<T> = std::result::Result<T, String>;

pub(crate) fn fail<T>(message: impl Into<String>) -> Result<T> {
    Err(message.into())
}

pub(crate) fn os(error: io::Error) -> String {
    error.to_string()
}

/// Paths and environment for launching terminal runtimes.
pub(crate) struct Launch {
    pub runtime: PathBuf,
    pub cwd: PathBuf,
    pub agent_bin: Option<PathBuf>,
}

pub(crate) fn uid() -> u32 {
    // SAFETY: getuid has no preconditions and cannot fail.
    unsafe { libc::getuid() }
}

pub(crate) fn random_hex(bytes: usize) -> Result<String> {
    let mut buffer = vec![0; bytes];
    File::open("/dev/urandom")
        .and_then(|mut file| file.read_exact(&mut buffer))
        .map_err(os)?;
    Ok(buffer.iter().map(|byte| format!("{byte:02x}")).collect())
}

fn has_control(value: &str) -> bool {
    value.chars().any(|ch| (ch as u32) < 32 || ch as u32 == 127)
}

pub(crate) fn name(value: &Value) -> Result<String> {
    let Some(text) = value.as_str().filter(|text| !text.trim().is_empty()) else {
        return fail("Enter a name");
    };
    if text.len() > 63 || has_control(text) {
        return fail("Names must fit 63 UTF-8 bytes and contain no control characters");
    }
    Ok(text.trim().to_owned())
}

pub(crate) fn expand_user(value: &str) -> PathBuf {
    let home = || std::env::var_os("HOME").map(PathBuf::from);
    if value == "~" {
        if let Some(home) = home() {
            return home;
        }
    } else if let Some(rest) = value.strip_prefix("~/") {
        if let Some(home) = home() {
            return home.join(rest);
        }
    }
    PathBuf::from(value)
}

/// Non-strict resolution: canonicalize the longest existing ancestor and
/// normalize the remaining components lexically.
pub(crate) fn resolve(path: &Path) -> PathBuf {
    if let Ok(resolved) = fs::canonicalize(path) {
        return resolved;
    }
    let mut existing = path.to_path_buf();
    let mut rest = Vec::new();
    while !existing.exists() {
        match (existing.file_name(), existing.parent()) {
            (Some(name), Some(parent)) => {
                rest.push(name.to_owned());
                existing = parent.to_path_buf();
            }
            _ => break,
        }
    }
    let mut resolved = fs::canonicalize(&existing).unwrap_or(existing);
    for name in rest.iter().rev() {
        resolved.push(name);
    }
    let mut normalized = PathBuf::new();
    for component in resolved.components() {
        match component {
            Component::ParentDir => {
                normalized.pop();
            }
            Component::CurDir => {}
            other => normalized.push(other),
        }
    }
    normalized
}

pub(crate) fn directory_value(value: &str, base: Option<&Path>, available: bool) -> Result<String> {
    if value.is_empty() || value.len() > 1023 || has_control(value) {
        return fail("Enter a folder path of at most 1023 bytes without control characters");
    }
    let mut path = expand_user(value);
    if let Some(base) = base {
        if !path.is_absolute() {
            path = base.join(path);
        }
        path = resolve(&path);
    }
    if !path.is_absolute() {
        return fail("Saved workspace folder must be absolute");
    }
    if path.as_os_str().len() > 1023 {
        return fail("Resolved folder path exceeds 1023 bytes");
    }
    if available && (!path.is_dir() || !accessible(&path)) {
        return fail("Folder does not exist or cannot be accessed");
    }
    Ok(path.to_string_lossy().into_owned())
}

fn accessible(path: &Path) -> bool {
    let Ok(text) = std::ffi::CString::new(path.as_os_str().as_bytes()) else {
        return false;
    };
    // SAFETY: text is a valid NUL-terminated path for the duration of the call.
    unsafe { libc::access(text.as_ptr(), libc::R_OK | libc::X_OK) == 0 }
}

/// An exclusive advisory lock, released when dropped.
pub(crate) struct Lock(#[allow(dead_code)] File);

pub(crate) fn lock_file(path: &Path, nonblocking: bool) -> io::Result<File> {
    let file = OpenOptions::new().append(true).create(true).open(path)?;
    let mode = libc::LOCK_EX | if nonblocking { libc::LOCK_NB } else { 0 };
    // SAFETY: the descriptor is open for the lifetime of `file`.
    if unsafe { libc::flock(file.as_raw_fd(), mode) } != 0 {
        return Err(io::Error::last_os_error());
    }
    Ok(file)
}

pub(crate) fn locked(directory: &Path) -> Result<Lock> {
    DirBuilder::new()
        .recursive(true)
        .mode(0o700)
        .create(directory)
        .map_err(os)?;
    Ok(Lock(
        lock_file(&directory.join("catalog.lock"), false).map_err(os)?,
    ))
}

pub(crate) fn atomic_write(path: &Path, value: &str) -> Result<()> {
    let parent = path.parent().unwrap_or(Path::new("."));
    let prefix = path.file_name().unwrap_or_default().to_string_lossy();
    let (temporary, mut file) = loop {
        let candidate = parent.join(format!("{prefix}.{}", random_hex(4)?));
        match OpenOptions::new()
            .write(true)
            .create_new(true)
            .mode(0o600)
            .open(&candidate)
        {
            Ok(file) => break (candidate, file),
            Err(error) if error.kind() == io::ErrorKind::AlreadyExists => continue,
            Err(error) => return Err(os(error)),
        }
    };
    let result = (|| -> io::Result<()> {
        file.write_all(value.as_bytes())?;
        file.sync_all()?;
        fs::rename(&temporary, path)?;
        // Persist the renamed catalog/intent, where directory fsync is supported.
        match File::open(parent)?.sync_all() {
            Err(error)
                if matches!(
                    error.raw_os_error(),
                    Some(libc::EINVAL) | Some(libc::ENOTSUP)
                ) =>
            {
                Ok(())
            }
            other => other,
        }
    })();
    let _ = fs::remove_file(&temporary);
    result.map_err(os)
}

pub(crate) fn id(entity: &Value) -> &str {
    entity["id"].as_str().unwrap_or("")
}

pub(crate) fn workspaces(data: &Value) -> &[Value] {
    data["workspaces"].as_array().map_or(&[], Vec::as_slice)
}

pub(crate) fn workspaces_mut(data: &mut Value) -> &mut Vec<Value> {
    if !data["workspaces"].is_array() {
        data["workspaces"] = json!([]);
    }
    data["workspaces"].as_array_mut().expect("workspaces array")
}

pub(crate) fn terminals(workspace: &Value) -> &[Value] {
    workspace["terminals"].as_array().map_or(&[], Vec::as_slice)
}

fn terminals_mut(workspace: &mut Value) -> &mut Vec<Value> {
    workspace["terminals"]
        .as_array_mut()
        .expect("validated terminals")
}

pub(crate) fn terminal_ids(data: &Value) -> Vec<String> {
    workspaces(data)
        .iter()
        .flat_map(terminals)
        .map(|terminal| id(terminal).to_owned())
        .collect()
}

pub(crate) fn find_terminal<'a>(data: &'a Value, identity: &str) -> Option<(&'a Value, &'a Value)> {
    workspaces(data).iter().find_map(|workspace| {
        terminals(workspace)
            .iter()
            .find(|terminal| id(terminal) == identity)
            .map(|terminal| (workspace, terminal))
    })
}

fn seed(directory: &Path) -> Result<Value> {
    let mut defaults: Vec<(String, Vec<String>)> = vec![
        ("Main".into(), vec!["spike".into(), "main-2".into()]),
        (
            "Scratch".into(),
            vec!["scratch-1".into(), "scratch-2".into()],
        ),
    ];
    let legacy = directory.join("workspaces.tsv");
    if legacy.exists() {
        let text = fs::read_to_string(&legacy).map_err(os)?;
        let lines: Vec<&str> = text.lines().collect();
        if lines.first().is_some_and(|line| {
            matches!(
                *line,
                "workbench-catalog-v2" | "workbench-catalog-v3" | "workbench-catalog-v4"
            )
        }) {
            return fail(
                "Saved catalog found without JSON metadata; existing files were preserved",
            );
        }
        if !lines.is_empty() {
            defaults.clear();
            let base = resolve(directory);
            for row in lines {
                let fields: Vec<&str> = row.split('\t').collect();
                if fields.len() != 3 {
                    return fail("Invalid old catalog; its contents were preserved");
                }
                let mut identities = Vec::new();
                for field in &fields[1..] {
                    let path = Path::new(field);
                    if resolve(path).parent() != Some(base.as_path())
                        || path.extension().is_none_or(|suffix| suffix != "sock")
                    {
                        return fail("Old catalog paths must belong to this session directory");
                    }
                    identities.push(
                        path.file_stem()
                            .unwrap_or_default()
                            .to_string_lossy()
                            .into_owned(),
                    );
                }
                defaults.push((name(&json!(fields[0]))?, identities));
            }
        }
    }
    let workspaces: Vec<Value> = defaults
        .into_iter()
        .enumerate()
        .map(|(index, (title, identities))| {
            let terminals: Vec<Value> = identities
                .into_iter()
                .enumerate()
                .map(|(position, identity)| {
                    json!({"id": identity, "name": format!("Shell {}", position + 1)})
                })
                .collect();
            json!({"id": format!("workspace-{index}"), "name": title, "terminals": terminals})
        })
        .collect();
    Ok(json!({"version": 1, "workspaces": workspaces}))
}

fn valid_identity(identity: &str) -> bool {
    !identity.is_empty()
        && identity.len() <= 40
        && identity
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || byte == b'-' || byte == b'_')
}

pub(crate) fn validate(data: &Value) -> Result<()> {
    if !data.is_object() || data.get("version").and_then(Value::as_f64) != Some(1.0) {
        return fail("Unsupported workspace metadata version");
    }
    let Some(workspaces) = data["workspaces"]
        .as_array()
        .filter(|list| list.len() <= MAX_WORKSPACES)
    else {
        return fail("Invalid workspace count");
    };
    let mut ids = std::collections::HashSet::new();
    for workspace in workspaces {
        if let Some(directory) = workspace.get("directory") {
            directory_value(directory.as_str().unwrap_or(""), None, false)?;
        }
        if !workspace["terminals"]
            .as_array()
            .is_some_and(|list| list.len() <= MAX_TERMINALS)
        {
            return fail("Invalid terminal count");
        }
        for entity in std::iter::once(workspace).chain(terminals(workspace)) {
            if !entity.is_object() {
                return fail("Invalid session metadata");
            }
            let identity = entity.get("id").and_then(Value::as_str).unwrap_or("");
            if !valid_identity(identity) || !ids.insert(identity.to_owned()) {
                return fail("Invalid or duplicate session identity");
            }
            if Value::String(name(&entity["name"])?) != entity["name"] {
                return fail("Invalid saved name");
            }
            if entity
                .get("custom_name")
                .is_some_and(|flag| !flag.is_boolean())
            {
                return fail("Invalid custom terminal name flag");
            }
        }
    }
    validate_pending(data)
}

fn validate_pending(data: &Value) -> Result<()> {
    let terminal_set: std::collections::HashSet<String> = terminal_ids(data).into_iter().collect();
    let workspace_set: std::collections::HashSet<&str> = workspaces(data).iter().map(id).collect();
    let keys = ["pending_create", "pending_close", "pending_stop"];
    if keys.iter().filter(|key| data.get(**key).is_some()).count() > 1 {
        return fail("Conflicting pending catalog operations");
    }
    let identities = |value: &Value, known: &dyn Fn(&str) -> bool| -> Result<usize> {
        let Some(list) = value.as_array() else {
            return fail("Invalid pending catalog identities");
        };
        let mut seen = std::collections::HashSet::new();
        for item in list {
            match item.as_str() {
                Some(text) if known(text) && seen.insert(text) => {}
                _ => return fail("Invalid pending catalog identities"),
            }
        }
        Ok(list.len())
    };
    let terminal_known = |text: &str| terminal_set.contains(text);
    if let Some(value) = data.get("pending_stop") {
        if identities(value, &terminal_known)? == 0 {
            return fail("Empty pending shutdown");
        }
    }
    if let Some(value) = data.get("pending_create") {
        if identities(value, &terminal_known)? == 0 {
            return fail("Empty pending creation");
        }
    }
    if let Some(pending) = data.get("pending_close") {
        let Some(object) = pending.as_object().filter(|object| {
            object.len() == 2
                && object.contains_key("terminals")
                && object.contains_key("workspaces")
        }) else {
            return fail("Invalid pending close metadata");
        };
        let closing = identities(&object["terminals"], &terminal_known)?;
        let workspace_known = |text: &str| workspace_set.contains(text);
        let removed = identities(&object["workspaces"], &workspace_known)?;
        if closing == 0 && removed == 0 {
            return fail("Empty pending close");
        }
        let listed = |identity: &str| {
            object["terminals"]
                .as_array()
                .is_some_and(|list| list.iter().any(|item| item.as_str() == Some(identity)))
        };
        for workspace in workspaces(data) {
            let selected = object["workspaces"]
                .as_array()
                .is_some_and(|list| list.iter().any(|item| item.as_str() == Some(id(workspace))));
            if selected && terminals(workspace).iter().any(|t| !listed(id(t))) {
                return fail("Pending workspace close omits terminals");
            }
        }
    }
    Ok(())
}

pub(crate) fn load(directory: &Path) -> Result<Value> {
    let path = directory.join("workspaces.json");
    let data = if path.exists() {
        let text = fs::read_to_string(&path).map_err(os)?;
        serde_json::from_str(&text).map_err(|error| error.to_string())?
    } else {
        seed(directory)?
    };
    validate(&data)?;
    Ok(data)
}

pub(crate) fn socket_path(directory: &Path, identity: &str) -> Result<PathBuf> {
    let path = directory.join(format!("{identity}.sock"));
    let text = path.to_string_lossy();
    if path.as_os_str().len() >= 104 || text.contains(['\t', '\r', '\n']) {
        return fail(format!("Session socket path exceeds 103 bytes or contains a tab/newline: {text}. Use a shorter checkout path"));
    }
    Ok(path)
}

/// Every saved terminal's socket path must fit the platform limit.
pub(crate) fn check_sockets(directory: &Path, data: &Value) -> Result<()> {
    for terminal in workspaces(data).iter().flat_map(terminals) {
        socket_path(directory, id(terminal))?;
    }
    Ok(())
}

/// Probe without sending input. Only absence/refusal establishes a dead endpoint.
pub(crate) fn endpoint_alive(path: &Path) -> Result<bool> {
    match UnixStream::connect(path) {
        Ok(_) => Ok(true),
        Err(error)
            if matches!(
                error.raw_os_error(),
                Some(libc::ENOENT) | Some(libc::ECONNREFUSED)
            ) =>
        {
            Ok(false)
        }
        Err(error) => fail(format!(
            "Could not check session endpoint {}: {error}",
            path.display()
        )),
    }
}

pub(crate) fn alive(socket: &Path) -> Result<bool> {
    Ok(endpoint_alive(&socket.with_extension("ctl"))? || endpoint_alive(socket)?)
}

/// Fails unless each existing endpoint is a socket owned by this user.
pub(crate) fn owned_sockets(socket: &Path, message: &str) -> Result<()> {
    for candidate in [socket.with_extension("ctl"), socket.to_path_buf()] {
        match fs::symlink_metadata(&candidate) {
            Ok(metadata) if metadata.file_type().is_socket() && metadata.uid() == uid() => {}
            Ok(_) => return fail(message),
            Err(error) if error.kind() == io::ErrorKind::NotFound => {}
            Err(error) => return Err(os(error)),
        }
    }
    Ok(())
}

/// Explicit recovery, under the catalog lock; never remove a listening endpoint.
pub(crate) fn recover_sessions(directory: &Path, data: &Value) -> Result<()> {
    let mut stale = Vec::new();
    for identity in terminal_ids(data) {
        let path = socket_path(directory, &identity)?;
        let paths = [path.with_extension("ctl"), path];
        let mut live = false;
        for endpoint in &paths {
            if endpoint_alive(endpoint)? {
                live = true;
                break;
            }
        }
        if live {
            continue;
        }
        for endpoint in paths {
            let metadata = match fs::symlink_metadata(&endpoint) {
                Ok(metadata) => metadata,
                Err(error) if error.kind() == io::ErrorKind::NotFound => continue,
                Err(error) => return Err(os(error)),
            };
            if !metadata.file_type().is_socket() || metadata.uid() != uid() {
                return fail(format!(
                    "Refusing to recover a non-socket or unowned endpoint: {}",
                    endpoint.display()
                ));
            }
            stale.push((endpoint, metadata.dev(), metadata.ino()));
        }
    }
    // Validate the whole plan before removing anything, then recheck each endpoint.
    let unchanged = |path: &Path, device: u64, inode: u64| -> Result<()> {
        let metadata = fs::symlink_metadata(path).map_err(os)?;
        if (metadata.dev(), metadata.ino()) != (device, inode) || endpoint_alive(path)? {
            return fail(format!(
                "Session endpoint changed during recovery: {}; retry",
                path.display()
            ));
        }
        Ok(())
    };
    for (path, device, inode) in &stale {
        unchanged(path, *device, *inode)?;
    }
    for (path, device, inode) in &stale {
        unchanged(path, *device, *inode)?;
        fs::remove_file(path).map_err(os)?;
    }
    Ok(())
}

fn now_ms() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_or(0, |duration| duration.as_millis())
}

fn prepare_launch(socket: &Path) -> Result<()> {
    // Reset previous evidence even when an older runtime will be launched.
    let path = socket.with_extension("lifecycle");
    match fs::symlink_metadata(&path) {
        Ok(metadata) if !metadata.file_type().is_file() || metadata.uid() != uid() => {
            return fail("Lifecycle path is not a user-owned regular file");
        }
        Err(error) if error.kind() != io::ErrorKind::NotFound => return Err(os(error)),
        _ => {}
    }
    let record = json!({"version": 1, "event": "launching", "runtime_pid": null,
        "shell_pid": null, "started_ms": now_ms() as u64, "exit_code": null, "signal": null});
    atomic_write(&path, &(record.to_string() + "\n"))
}

fn workspace_folder(workspace: &Value, launch: &Launch) -> Result<String> {
    let folder = workspace
        .get("directory")
        .and_then(Value::as_str)
        .map_or_else(|| launch.cwd.to_string_lossy().into_owned(), str::to_owned);
    directory_value(&folder, Some(&launch.cwd), true)
}

/// Start runtimes for terminals without an endpoint. Started, ready runtimes are
/// handed to `children` and survive a later failure; unready ones are stopped.
pub(crate) fn ensure_sessions(
    directory: &Path,
    data: &Value,
    launch: &Launch,
    children: &mut Vec<(String, Child)>,
) -> Result<()> {
    for workspace in workspaces(data) {
        for terminal in terminals(workspace) {
            let path = socket_path(directory, id(terminal))?;
            if path.exists() {
                if !alive(&path)? {
                    return fail(format!("Session {} has a socket but no listening server ({}). Run just recover-sessions, then just spike", id(terminal), path.display()));
                }
                continue;
            }
            prepare_launch(&path)?;
            let log_name = format!("{}.log", id(terminal));
            let log = OpenOptions::new()
                .append(true)
                .create(true)
                .open(directory.join(&log_name))
                .map_err(os)?;
            let mut command = Command::new(&launch.runtime);
            command
                .arg(&path)
                .current_dir(workspace_folder(workspace, launch)?)
                .stdin(Stdio::null())
                .stdout(log.try_clone().map_err(os)?)
                .stderr(log);
            for (key, _) in std::env::vars_os() {
                if key.as_bytes().starts_with(b"BAND_") {
                    command.env_remove(key);
                }
            }
            if let Some(bin) = &launch.agent_bin {
                command.env("BAND_AGENT_BIN", bin);
            }
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
            let deadline = Instant::now() + Duration::from_secs(5);
            while !path.exists() {
                let exited = child.try_wait().map_err(os)?.is_some();
                if exited || Instant::now() >= deadline {
                    if !exited {
                        terminate(&mut child);
                    }
                    return fail(format!("Shell failed to start; inspect {log_name}"));
                }
                thread::sleep(Duration::from_millis(20));
            }
            children.push((id(terminal).to_owned(), child));
        }
    }
    Ok(())
}

fn terminate(child: &mut Child) {
    // SAFETY: the pid belongs to an unreaped child, so it cannot be reused.
    unsafe { libc::kill(child.id() as libc::pid_t, libc::SIGTERM) };
    let deadline = Instant::now() + Duration::from_secs(5);
    while Instant::now() < deadline {
        if !matches!(child.try_wait(), Ok(None)) {
            return;
        }
        thread::sleep(Duration::from_millis(20));
    }
}

/// Run `runtime stop SOCKET`, returning its error text on failure.
pub(crate) fn stop_runtime(runtime: &Path, socket: &Path) -> Result<()> {
    let mut child = Command::new(runtime)
        .arg("stop")
        .arg(socket)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(os)?;
    let deadline = Instant::now() + Duration::from_secs(15);
    let status = loop {
        if let Some(status) = child.try_wait().map_err(os)? {
            break status;
        }
        if Instant::now() >= deadline {
            let _ = child.kill();
            let _ = child.wait();
            return fail(format!(
                "Command '{} stop {}' timed out after 15 seconds",
                runtime.display(),
                socket.display()
            ));
        }
        thread::sleep(Duration::from_millis(20));
    };
    if status.success() {
        return Ok(());
    }
    let mut stderr = String::new();
    if let Some(mut pipe) = child.stderr.take() {
        let _ = pipe.read_to_string(&mut stderr);
    }
    let stderr = stderr.trim();
    fail(if stderr.is_empty() {
        "Session stop failed"
    } else {
        stderr
    })
}

fn move_terminal(data: &mut Value, identity: &str, destination: &str, position: i64) -> Result<()> {
    let list = workspaces_mut(data);
    let Some(source) = list
        .iter()
        .position(|w| terminals(w).iter().any(|t| id(t) == identity))
    else {
        return fail("Terminal no longer exists");
    };
    let Some(target) = list.iter().position(|w| id(w) == destination) else {
        return fail("Destination workspace no longer exists");
    };
    let count = terminals(&list[target]).len();
    if source != target && count >= MAX_TERMINALS {
        return fail("Destination already has eight terminals");
    }
    let remaining = count - usize::from(source == target);
    let position = if position == -1 {
        remaining as i64
    } else {
        position
    };
    if !(0..=remaining as i64).contains(&position) {
        return fail("Invalid terminal insertion position");
    }
    let from = terminals(&list[source])
        .iter()
        .position(|t| id(t) == identity)
        .expect("located terminal");
    let terminal = terminals_mut(&mut list[source]).remove(from);
    terminals_mut(&mut list[target]).insert(position as usize, terminal);
    validate(data)
}

/// Keep new socket names short without changing persisted terminal IDs.
fn new_terminal_identity(data: &Value) -> Result<String> {
    let used: std::collections::HashSet<&str> = workspaces(data)
        .iter()
        .flat_map(|w| std::iter::once(w).chain(terminals(w)))
        .map(id)
        .collect();
    for _ in 0..32 {
        let identity = random_hex(8)?;
        if !used.contains(identity.as_str()) {
            return Ok(identity);
        }
    }
    fail("Could not allocate a unique terminal identity; retry")
}

fn mutate(
    data: &mut Value,
    method: &str,
    identity: &str,
    title: &str,
    destination: &str,
    position: i64,
) -> Result<()> {
    if method == "terminal.move" {
        return move_terminal(data, identity, destination, position);
    }
    let title = name(&json!(title))?;
    match method {
        "workspace.create" => {
            if workspaces(data).len() >= MAX_WORKSPACES {
                return fail("This spike supports eight workspaces");
            }
            let terminal = new_terminal_identity(data)?;
            let workspace = json!({"id": random_hex(16)?, "name": title,
                "terminals": [{"id": terminal, "name": "Shell 1"}]});
            workspaces_mut(data).push(workspace);
        }
        "workspace.rename" | "terminal.create" => {
            let terminal = new_terminal_identity(data)?;
            let Some(workspace) = workspaces_mut(data).iter_mut().find(|w| id(w) == identity)
            else {
                return fail("Workspace no longer exists");
            };
            if method == "workspace.rename" {
                workspace["name"] = json!(title);
            } else {
                if terminals(workspace).len() >= MAX_TERMINALS {
                    return fail("This spike supports eight terminals per workspace");
                }
                terminals_mut(workspace).push(json!({"id": terminal, "name": title}));
            }
        }
        "terminal.rename" => {
            let Some(terminal) = workspaces_mut(data)
                .iter_mut()
                .flat_map(|w| terminals_mut(w).iter_mut())
                .find(|t| id(t) == identity)
            else {
                return fail("Terminal no longer exists");
            };
            terminal["name"] = json!(title);
            terminal["custom_name"] = json!(true);
        }
        _ => return fail("Unknown management action"),
    }
    validate(data)
}

fn catalog_json(data: &Value) -> String {
    serde_json::to_string_pretty(data).expect("JSON values serialize") + "\n"
}

pub(crate) fn save(directory: &Path, data: &Value) -> Result<()> {
    validate(data)?;
    check_sockets(directory, data)?;
    atomic_write(&directory.join("workspaces.json"), &catalog_json(data))
}

/// Reconcile durable creation intent without starting or stopping a shell.
pub(crate) fn finish_create(directory: &Path, data: &mut Value) -> Result<bool> {
    validate(data)?;
    // A saved identity survives whether its launch succeeded, failed or never ran.
    // Live endpoints are adopted normally; absent ones require explicit recovery.
    if data
        .as_object_mut()
        .and_then(|o| o.remove("pending_create"))
        .is_none()
    {
        return Ok(false);
    }
    save(directory, data)?;
    Ok(true)
}

fn string_list(value: &Value) -> Vec<String> {
    value
        .as_array()
        .map(|list| {
            list.iter()
                .filter_map(Value::as_str)
                .map(str::to_owned)
                .collect()
        })
        .unwrap_or_default()
}

pub(crate) fn finish_close(directory: &Path, runtime: &Path, data: &mut Value) -> Result<()> {
    validate(data)?;
    let Some(pending) = data.get("pending_close").cloned() else {
        return Ok(());
    };
    let ids = string_list(&pending["terminals"]);
    let removed = string_list(&pending["workspaces"]);
    for identity in &ids {
        let path = socket_path(directory, identity)?;
        if let Err(error) = stop_runtime(runtime, &path) {
            return fail(format!("Close pending; reopen to retry. {error}"));
        }
    }
    let list = workspaces_mut(data);
    for workspace in list.iter_mut() {
        terminals_mut(workspace).retain(|t| !ids.iter().any(|i| i == id(t)));
    }
    list.retain(|w| !removed.iter().any(|i| i == id(w)));
    data.as_object_mut()
        .expect("catalog object")
        .remove("pending_close");
    save(directory, data)
}

/// Check one endpoint before shutdown: returns its socket path and liveness.
fn check_endpoint(directory: &Path, identity: &str) -> Result<(PathBuf, bool)> {
    let path = socket_path(directory, identity)?;
    owned_sockets(
        &path,
        &format!("Session endpoint is not a user-owned socket: {identity}"),
    )?;
    let live = alive(&path)?;
    if live && (!path.exists() || !path.with_extension("ctl").exists()) {
        return fail(format!(
            "Terminal cannot be stopped through this runtime: {identity}"
        ));
    }
    Ok((path, live))
}

/// Finish explicit bounded shutdown of confirmed identities; organization survives.
pub(crate) fn finish_stop(directory: &Path, runtime: &Path, data: &mut Value) -> Result<()> {
    validate(data)?;
    let Some(targets) = data.get("pending_stop").map(string_list) else {
        return Ok(());
    };
    // Explicit lifecycle work, never a render/PTY path. Bound parallelism and
    // per-process deadlines so a full catalog does not wait serially per shell.
    let stop = |identity: &String| -> Option<String> {
        let result = check_endpoint(directory, identity).and_then(|(path, live)| {
            if live {
                stop_runtime(runtime, &path)
            } else {
                Ok(())
            }
        });
        result.err().map(|error| format!("{identity}: {error}"))
    };
    let mut failures = Vec::new();
    for chunk in targets.chunks(32) {
        thread::scope(|scope| {
            let handles: Vec<_> = chunk
                .iter()
                .map(|identity| scope.spawn(|| stop(identity)))
                .collect();
            failures.extend(
                handles
                    .into_iter()
                    .filter_map(|handle| handle.join().ok().flatten()),
            );
        });
    }
    if let Some(first) = failures.first() {
        return fail(format!(
            "Terminal shutdown pending; retry to finish. {first}"
        ));
    }
    data.as_object_mut()
        .expect("catalog object")
        .remove("pending_stop");
    save(directory, data)
}

pub(crate) fn begin_stop(
    directory: &Path,
    runtime: &Path,
    data: &mut Value,
    targets: &Value,
) -> Result<()> {
    let known = terminal_ids(data);
    let Some(list) = targets
        .as_array()
        .filter(|list| !list.is_empty() && list.len() <= MAX_WORKSPACES * MAX_TERMINALS)
    else {
        return fail("Choose terminals to stop");
    };
    let mut selected = Vec::new();
    for item in list {
        match item.as_str() {
            Some(text) if known.iter().any(|k| k == text) && !selected.contains(&text) => {
                selected.push(text)
            }
            _ => return fail("Terminal selection changed; reopen the confirmation"),
        }
    }
    // Validate the entire selection before recording intent or stopping anything.
    for identity in &selected {
        check_endpoint(directory, identity)?;
    }
    data["pending_stop"] = json!(selected);
    save(directory, data)?;
    finish_stop(directory, runtime, data)
}

fn close_sessions(
    directory: &Path,
    runtime: &Path,
    data: &mut Value,
    method: &str,
    identity: &str,
) -> Result<()> {
    let workspace_close = method == "workspace.close";
    let affected: Vec<String> = workspaces(data)
        .iter()
        .flat_map(|w| terminals(w).iter().map(move |t| (w, t)))
        .filter(|(w, t)| {
            if workspace_close {
                id(w) == identity
            } else {
                id(t) == identity
            }
        })
        .map(|(_, t)| id(t).to_owned())
        .collect();
    if workspace_close {
        if !workspaces(data).iter().any(|w| id(w) == identity) {
            return fail("Workspace no longer exists");
        }
    } else if affected.is_empty() {
        return fail("Terminal no longer exists");
    }
    // Check every endpoint before recording destructive intent or stopping any shell.
    for terminal in &affected {
        let path = socket_path(directory, terminal)?;
        if path.exists() && !path.with_extension("ctl").exists() {
            return fail("Earlier spike session: exit its shell and reopen before using Stop");
        }
    }
    let removed: Vec<&str> = if workspace_close {
        vec![identity]
    } else {
        vec![]
    };
    data["pending_close"] = json!({"terminals": affected, "workspaces": removed});
    save(directory, data)?;
    finish_close(directory, runtime, data)
}

/// One locked organization transaction, matching the original management CLI.
#[allow(clippy::too_many_arguments)]
pub(crate) fn manage(
    directory: &Path,
    launch: &Launch,
    method: &str,
    identity: &str,
    title: &str,
    destination: &str,
    position: i64,
    project_directory: Option<&str>,
    children: &mut Vec<(String, Child)>,
) -> Result<Value> {
    let _lock = locked(directory)?;
    let saved_catalog = directory.join("workspaces.json").exists();
    let mut data = load(directory)?;
    finish_create(directory, &mut data)?;
    finish_stop(directory, &launch.runtime, &mut data)?;
    let explicit_recovery = method == "session.recover";
    let mut method = method.to_owned();
    // Recover interrupted close before any startup, never resurrect its shells.
    if explicit_recovery {
        recover_sessions(directory, &data)?;
        method = "initialize".into();
    }
    finish_close(directory, &launch.runtime, &mut data)?;
    if method == "terminal.close" || method == "workspace.close" {
        close_sessions(directory, &launch.runtime, &mut data, &method, identity)?;
        return Ok(data);
    }
    let old_ids = terminal_ids(&data);
    // Directory changes are organization facts; never send cd to live shells.
    if method == "workspace.directory" || method == "workspace.create_at" {
        let Some(requested) = project_directory else {
            return fail("Enter a folder path of at most 1023 bytes without control characters");
        };
        let selected = directory_value(requested, Some(&launch.cwd), true)?;
        if method == "workspace.directory" {
            let Some(workspace) = workspaces_mut(&mut data)
                .iter_mut()
                .find(|w| id(w) == identity)
            else {
                return fail("Workspace no longer exists");
            };
            workspace["directory"] = json!(selected);
            save(directory, &data)?;
            return Ok(data);
        }
        mutate(&mut data, "workspace.create", identity, title, "", -1)?;
        workspaces_mut(&mut data)
            .last_mut()
            .expect("created workspace")["directory"] = json!(selected);
        method = "initialize-new".into();
    }
    if method != "initialize" && method != "initialize-new" {
        mutate(&mut data, &method, identity, title, destination, position)?;
    }
    check_sockets(directory, &data)?; // Before committing intent.
    let startup = if method == "initialize" {
        data.clone()
    } else {
        let mut subset = data.clone();
        for workspace in workspaces_mut(&mut subset) {
            terminals_mut(workspace).retain(|t| !old_ids.iter().any(|i| i == id(t)));
        }
        subset
    };
    let mut launch_new =
        method == "initialize" || method.ends_with(".create") || method == "initialize-new";
    if saved_catalog && method == "initialize" && !explicit_recovery {
        launch_new = false;
    }
    let launch_ids = if launch_new {
        terminal_ids(&startup)
    } else {
        vec![]
    };
    if !launch_ids.is_empty() {
        // Commit identities BEFORE spawning; a crash leaves recoverable intent.
        // A partial launch must not kill successful independent shells.
        for workspace in workspaces(&startup) {
            if !terminals(workspace).is_empty() {
                workspace_folder(workspace, launch)?;
            }
        }
        data["pending_create"] = json!(launch_ids);
        validate(&data)?;
        atomic_write(&directory.join("workspaces.json"), &catalog_json(&data))?;
        ensure_sessions(directory, &startup, launch, children)?;
        data.as_object_mut()
            .expect("catalog object")
            .remove("pending_create");
    }
    save(directory, &data)?;
    Ok(data)
}

/// Organization with an empty object for missing catalogs, used by health.
pub(crate) fn empty() -> Value {
    Value::Object(Map::from_iter([("workspaces".to_owned(), json!([]))]))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn temporary(label: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!("wb-catalog-{label}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        root
    }

    fn sample() -> Value {
        json!({"version": 1, "extra": "kept", "workspaces": [
            {"id": "w1", "name": "One", "terminals": [{"id": "a", "name": "Shell 1"}, {"id": "b", "name": "Logs"}]},
            {"id": "w2", "name": "Two", "terminals": []}]})
    }

    #[test]
    fn validation_rejects_unsafe_or_ambiguous_metadata() {
        assert!(validate(&sample()).is_ok());
        let mut duplicate = sample();
        duplicate["workspaces"][1]["id"] = json!("a");
        assert_eq!(
            validate(&duplicate).unwrap_err(),
            "Invalid or duplicate session identity"
        );
        let mut padded = sample();
        padded["workspaces"][0]["name"] = json!(" One");
        assert_eq!(validate(&padded).unwrap_err(), "Invalid saved name");
        let mut relative = sample();
        relative["workspaces"][0]["directory"] = json!("projects");
        assert_eq!(
            validate(&relative).unwrap_err(),
            "Saved workspace folder must be absolute"
        );
        let mut conflicting = sample();
        conflicting["pending_create"] = json!(["a"]);
        conflicting["pending_stop"] = json!(["b"]);
        assert_eq!(
            validate(&conflicting).unwrap_err(),
            "Conflicting pending catalog operations"
        );
        let mut partial = sample();
        partial["pending_close"] = json!({"terminals": ["a"], "workspaces": ["w1"]});
        assert_eq!(
            validate(&partial).unwrap_err(),
            "Pending workspace close omits terminals"
        );
        assert_eq!(
            name(&json!("tab\there")).unwrap_err(),
            "Names must fit 63 UTF-8 bytes and contain no control characters"
        );
    }

    #[test]
    fn moves_respect_positions_and_capacity() {
        let mut data = sample();
        move_terminal(&mut data, "b", "w1", 0).unwrap();
        assert_eq!(terminal_ids(&data), ["b", "a"]);
        move_terminal(&mut data, "b", "w2", -1).unwrap();
        assert_eq!(id(&terminals(&workspaces(&data)[1])[0]), "b");
        assert_eq!(
            move_terminal(&mut data, "a", "w2", 3).unwrap_err(),
            "Invalid terminal insertion position"
        );
        for index in 0..7 {
            data["workspaces"][1]["terminals"]
                .as_array_mut()
                .unwrap()
                .push(json!({"id": format!("x{index}"), "name": "Shell 1"}));
        }
        assert_eq!(
            move_terminal(&mut data, "a", "w2", -1).unwrap_err(),
            "Destination already has eight terminals"
        );
    }

    #[test]
    fn save_round_trips_order_and_unknown_fields() {
        let root = temporary("save");
        let mut data = sample();
        data["workspaces"][0]["directory"] = json!("/tmp");
        save(&root, &data).unwrap();
        let text = fs::read_to_string(root.join("workspaces.json")).unwrap();
        assert!(text.starts_with("{\n  \"version\": 1,\n  \"extra\": \"kept\","));
        assert_eq!(load(&root).unwrap(), data);
        assert!(!root.join("workspaces.tsv").exists());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn folders_resolve_against_the_base_without_requiring_existence() {
        let root = temporary("folders");
        let base = fs::canonicalize(&root).unwrap();
        fs::create_dir(base.join("real")).unwrap();
        assert_eq!(
            directory_value("real/../real", Some(&base), true).unwrap(),
            base.join("real").to_string_lossy()
        );
        assert_eq!(
            directory_value("missing/child", Some(&base), false).unwrap(),
            base.join("missing/child").to_string_lossy()
        );
        assert_eq!(
            directory_value("missing", Some(&base), true).unwrap_err(),
            "Folder does not exist or cannot be accessed"
        );
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn creation_allocates_short_unique_terminal_identities() {
        let mut data = sample();
        mutate(&mut data, "terminal.create", "w2", "Build", "", -1).unwrap();
        let created = &terminals(&workspaces(&data)[1])[0];
        assert_eq!(id(created).len(), 16);
        assert_eq!(created["name"], "Build");
        mutate(&mut data, "terminal.rename", "a", "Server", "", -1).unwrap();
        assert_eq!(find_terminal(&data, "a").unwrap().1["custom_name"], true);
        assert_eq!(
            mutate(&mut data, "workspace.rename", "gone", "X", "", -1).unwrap_err(),
            "Workspace no longer exists"
        );
    }

    fn manage_saved(root: &Path, method: &str, identity: &str, title: &str) -> Result<Value> {
        // A missing runtime proves these transactions never launch a shell.
        let launch = Launch {
            runtime: root.join("no-runtime"),
            cwd: root.to_owned(),
            agent_bin: None,
        };
        manage(
            root,
            &launch,
            method,
            identity,
            title,
            "",
            -1,
            None,
            &mut Vec::new(),
        )
    }

    fn move_saved(root: &Path, identity: &str, destination: &str, position: i64) -> Result<Value> {
        let launch = Launch {
            runtime: root.join("no-runtime"),
            cwd: root.to_owned(),
            agent_bin: None,
        };
        let mut children = Vec::new();
        let result = manage(
            root,
            &launch,
            "terminal.move",
            identity,
            "",
            destination,
            position,
            None,
            &mut children,
        );
        assert!(children.is_empty());
        result
    }

    #[test]
    fn legacy_tsv_seeds_stable_identities() {
        let root = temporary("seed");
        let base = resolve(&root);
        fs::write(
            root.join("workspaces.tsv"),
            format!(
                "Main\t{0}/spike.sock\t{0}/main-2.sock\nScratch\t{0}/scratch-1.sock\t{0}/scratch-2.sock\n",
                base.display()
            ),
        )
        .unwrap();
        let data = seed(&root).unwrap();
        assert_eq!(
            terminal_ids(&data),
            ["spike", "main-2", "scratch-1", "scratch-2"]
        );
        assert_eq!(workspaces(&data)[1]["name"], "Scratch");
        fs::write(
            root.join("workspaces.tsv"),
            "Main\t/elsewhere/a.sock\t/elsewhere/b.sock\n",
        )
        .unwrap();
        assert_eq!(
            seed(&root).unwrap_err(),
            "Old catalog paths must belong to this session directory"
        );
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn renames_persist_and_invalid_actions_leave_the_catalog_unchanged() {
        let root = temporary("rename");
        save(&root, &sample()).unwrap();
        manage_saved(&root, "workspace.rename", "w1", "Projects").unwrap();
        let renamed = manage_saved(&root, "terminal.rename", "a", "Builder").unwrap();
        assert_eq!(workspaces(&renamed)[0]["name"], "Projects");
        assert_eq!(find_terminal(&renamed, "a").unwrap().1["custom_name"], true);
        assert_eq!(load(&root).unwrap(), renamed);
        // Choosing a generic name explicitly still marks it custom.
        let generic = manage_saved(&root, "terminal.rename", "b", "Shell 1").unwrap();
        assert_eq!(find_terminal(&generic, "b").unwrap().1["custom_name"], true);
        let before = fs::read(root.join("workspaces.json")).unwrap();
        for (method, identity, title) in [
            ("terminal.rename", "missing", "Name"),
            ("workspace.rename", "w1", ""),
            ("workspace.create", "", "bad\nname"),
            ("terminal.close", "missing", ""),
            ("workspace.close", "missing", ""),
        ] {
            assert!(manage_saved(&root, method, identity, title).is_err());
            assert_eq!(fs::read(root.join("workspaces.json")).unwrap(), before);
        }
        // Newer or corrupt metadata is preserved, never reseeded.
        fs::write(root.join("workspaces.json"), r#"{"version": 99}"#).unwrap();
        assert_eq!(
            manage_saved(&root, "initialize", "", "").unwrap_err(),
            "Unsupported workspace metadata version"
        );
        assert_eq!(
            fs::read_to_string(root.join("workspaces.json")).unwrap(),
            r#"{"version": 99}"#
        );
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn earlier_runtime_endpoints_are_rejected_before_close_intent() {
        let root = temporary("legacy-close");
        save(&root, &sample()).unwrap();
        fs::write(root.join("a.sock"), "").unwrap();
        let before = fs::read(root.join("workspaces.json")).unwrap();
        assert!(manage_saved(&root, "workspace.close", "w1", "")
            .unwrap_err()
            .starts_with("Earlier spike session"));
        assert_eq!(fs::read(root.join("workspaces.json")).unwrap(), before);
        fs::remove_file(root.join("a.sock")).unwrap();
        // A failed stop keeps the close intent; the next transaction finishes it
        // before anything else and never resurrects the closed shells.
        assert!(manage_saved(&root, "workspace.close", "w1", "")
            .unwrap_err()
            .starts_with("Close pending"));
        assert_eq!(
            load(&root).unwrap()["pending_close"]["terminals"],
            json!(["a", "b"])
        );
        let launch = Launch {
            runtime: PathBuf::from("/usr/bin/true"),
            cwd: root.clone(),
            agent_bin: None,
        };
        let mut children = Vec::new();
        let recovered = manage(
            &root,
            &launch,
            "initialize",
            "",
            "",
            "",
            -1,
            None,
            &mut children,
        )
        .unwrap();
        assert!(children.is_empty());
        assert!(recovered.get("pending_close").is_none());
        assert_eq!(workspaces(&recovered).len(), 1);
        assert_eq!(load(&root).unwrap(), recovered);
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn moves_keep_identities_and_reject_invalid_targets_without_saving() {
        let root = temporary("move");
        let full: Vec<Value> = (0..8)
            .map(|i| json!({"id": format!("f{i}"), "name": format!("Full {i}")}))
            .collect();
        save(
            &root,
            &json!({"version": 1, "workspaces": [
                {"id": "a", "name": "Source", "terminals": [{"id": "one", "name": "One"}, {"id": "two", "name": "Two"}]},
                {"id": "b", "name": "Empty", "terminals": []},
                {"id": "c", "name": "Full", "terminals": full}]}),
        )
        .unwrap();
        let moved = move_saved(&root, "one", "b", 0).unwrap();
        assert_eq!(
            terminals(&workspaces(&moved)[1]),
            [json!({"id": "one", "name": "One"})]
        );
        move_saved(&root, "two", "b", 0).unwrap();
        let moved = move_saved(&root, "two", "b", -1).unwrap();
        assert!(terminals(&workspaces(&moved)[0]).is_empty());
        let order: Vec<&str> = terminals(&workspaces(&moved)[1]).iter().map(id).collect();
        assert_eq!(order, ["one", "two"]);
        let before = fs::read(root.join("workspaces.json")).unwrap();
        for (identity, destination, position) in [
            ("missing", "a", 0),
            ("one", "missing", 0),
            ("one", "c", 0),
            ("one", "a", 1),
            ("one", "a", -2),
        ] {
            assert!(move_saved(&root, identity, destination, position).is_err());
            assert_eq!(fs::read(root.join("workspaces.json")).unwrap(), before);
        }
        // Reordering inside a full workspace still succeeds.
        let moved = move_saved(&root, "f0", "c", 7).unwrap();
        assert_eq!(id(terminals(&workspaces(&moved)[2]).last().unwrap()), "f0");
        let moved = move_saved(&root, "f0", "c", 0).unwrap();
        assert_eq!(id(&terminals(&workspaces(&moved)[2])[0]), "f0");
        assert_eq!(load(&root).unwrap(), moved);
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn capacity_identities_and_pending_intents_are_validated() {
        let mut data = sample();
        while terminals(&workspaces(&data)[0]).len() < MAX_TERMINALS {
            mutate(&mut data, "terminal.create", "w1", "Terminal", "", -1).unwrap();
        }
        assert!(mutate(&mut data, "terminal.create", "w1", "Too many", "", -1).is_err());
        let mut legacy = sample();
        legacy["workspaces"][0]["terminals"][0]["id"] = json!("a".repeat(32));
        assert!(
            validate(&legacy).is_ok(),
            "older 32-character IDs stay valid"
        );
        for value in [
            json!([]),
            json!(["missing"]),
            json!(["a", "a"]),
            json!([null]),
        ] {
            let mut invalid = sample();
            invalid["pending_stop"] = value;
            assert!(validate(&invalid).is_err());
        }
    }
}
