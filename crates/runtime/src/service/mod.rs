//! Session authority: one JSON request per connection on `service.rpc`, owning
//! the saved organization, creation/close journals, recovery and health.
//!
//! PTY streams remain with independent runtimes. Stopping this service never
//! stops them. Clients retry only connection establishment, never a sent mutation.
mod catalog;
mod client;
mod lifecycle;

pub(crate) use catalog::{expand_user, resolve};
use catalog::{fail, find_terminal, id, os, Launch, Result};
pub use client::{initialize, retire};
use serde_json::{json, Map, Value};
use std::collections::HashMap;
use std::fs::{self, DirBuilder};
use std::io::{self, Read, Write};
use std::os::unix::fs::{DirBuilderExt, FileTypeExt, MetadataExt, PermissionsExt};
use std::os::unix::io::AsRawFd;
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::{Path, PathBuf};
use std::process::Child;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

const PROTOCOL: i64 = 1;
// One request or reply; the Qt client and Python tools use the same bound.
pub(crate) const LIMIT: usize = 1024 * 1024;
const METHODS: [&str; 20] = [
    "catalog.read",
    "health",
    "initialize",
    "service.retire",
    "service.shutdown",
    "session.recover",
    "session.stop",
    "terminal.close",
    "terminal.create",
    "terminal.dismiss",
    "terminal.lifecycle",
    "terminal.move",
    "terminal.recover",
    "terminal.rename",
    "terminal.status",
    "workspace.close",
    "workspace.create",
    "workspace.create_at",
    "workspace.directory",
    "workspace.rename",
];

static RUNNING: AtomicBool = AtomicBool::new(true);

extern "C" fn interrupted(_: libc::c_int) {
    RUNNING.store(false, Ordering::SeqCst);
}

fn endpoint(directory: &Path) -> Result<PathBuf> {
    let path = directory.join("service.rpc");
    if path.as_os_str().len() >= 104 {
        return fail("Session service socket exceeds 103 bytes; use a shorter checkout path");
    }
    Ok(path)
}

/// Create the state directory, refusing one other users can write.
pub fn prepare_directory(directory: &Path) -> std::result::Result<(), String> {
    DirBuilder::new()
        .recursive(true)
        .mode(0o700)
        .create(directory)
        .map_err(os)?;
    let metadata = fs::metadata(directory).map_err(os)?;
    if metadata.uid() != catalog::uid() || metadata.mode() & 0o022 != 0 {
        return fail("Session directory must be owned by you and not writable by other users");
    }
    Ok(())
}

fn receive(connection: &mut UnixStream) -> Result<Value> {
    let mut data = Vec::new();
    let mut chunk = [0; 4096];
    while !data.contains(&b'\n') {
        let wanted = chunk.len().min(LIMIT + 1 - data.len());
        let count = connection.read(&mut chunk[..wanted]).map_err(os)?;
        if count == 0 {
            return fail("Session service closed before replying");
        }
        data.extend_from_slice(&chunk[..count]);
        if data.len() > LIMIT {
            return fail("Session service message exceeds size limit");
        }
    }
    let split = data
        .iter()
        .position(|byte| *byte == b'\n')
        .expect("newline");
    if split + 1 != data.len() {
        return fail("Only one request per connection is supported");
    }
    let value: Value = serde_json::from_slice(&data[..split]).map_err(|error| error.to_string())?;
    if !value.is_object() {
        return fail("Session service message must be an object");
    }
    Ok(value)
}

fn transmit(connection: &mut UnixStream, value: &Value) -> Result<()> {
    let mut payload = serde_json::to_vec(value).map_err(|error| error.to_string())?;
    payload.push(b'\n');
    if payload.len() > LIMIT {
        return fail("Session service reply exceeds size limit");
    }
    connection.write_all(&payload).map_err(os)
}

fn object(pairs: impl IntoIterator<Item = (&'static str, Value)>) -> Map<String, Value> {
    pairs
        .into_iter()
        .map(|(key, value)| (key.to_owned(), value))
        .collect()
}

fn pid() -> u32 {
    std::process::id()
}

struct Authority {
    directory: PathBuf,
    launch: Launch,
    instance: String,
    children: Vec<(String, Child)>,
    capabilities: HashMap<String, ((u64, u64, i64), Value)>,
    running: bool,
}

impl Authority {
    fn data_reply(&self, data: Value) -> Map<String, Value> {
        object([("data", data), ("service_pid", json!(pid()))])
    }

    fn reconcile(&self, data: &mut Value) -> Result<()> {
        catalog::finish_create(&self.directory, data)?;
        catalog::finish_stop(&self.directory, &self.launch.runtime, data)?;
        catalog::finish_close(&self.directory, &self.launch.runtime, data)
    }

    fn runtime_state(socket: &Path) -> &'static str {
        match catalog::alive(socket) {
            Ok(true) => "running",
            Ok(false) => "stopped",
            Err(_) => "unavailable",
        }
    }

    fn health(&mut self) -> Result<Map<String, Value>> {
        // Reading health never seeds the catalog or starts a terminal.
        let lock = catalog::locked(&self.directory)?;
        let data = if self.directory.join("workspaces.json").exists() {
            catalog::load(&self.directory)?
        } else {
            catalog::empty()
        };
        self.reap();
        let mut terminals = Vec::new();
        let mut controls = Vec::new();
        for workspace in catalog::workspaces(&data) {
            for terminal in catalog::terminals(workspace) {
                let path = catalog::socket_path(&self.directory, id(terminal))?;
                controls.push(path.with_extension("ctl").to_string_lossy().into_owned());
                let (state, mut issue) = match catalog::alive(&path) {
                    Ok(true) => ("running", String::new()),
                    Ok(false) => (
                        "stopped",
                        "Runtime stopped; explicit recovery starts a new shell".to_owned(),
                    ),
                    Err(error) => ("unavailable", error),
                };
                let compatibility = if state == "running" {
                    self.runtime_capabilities(&path)
                } else {
                    Value::Null
                };
                let detail = lifecycle::detail(&path, state);
                if state == "stopped" {
                    issue = detail["message"].as_str().unwrap_or_default().to_owned();
                }
                terminals.push(Value::Object(object([
                    ("lifecycle", detail),
                    ("id", terminal["id"].clone()),
                    ("workspace_id", workspace["id"].clone()),
                    ("workspace_name", workspace["name"].clone()),
                    ("name", terminal["name"].clone()),
                    ("state", json!(state)),
                    ("runtime", compatibility),
                    ("issue", json!(issue)),
                ])));
            }
        }
        drop(lock);
        self.capabilities.retain(|key, _| controls.contains(key));
        Ok(object([
            ("service_pid", json!(pid())),
            ("service_instance", json!(self.instance)),
            ("terminals", Value::Array(terminals)),
            (
                "runtime_binary",
                json!(self.launch.runtime.to_string_lossy()),
            ),
            ("directory", json!(self.launch.cwd.to_string_lossy())),
            ("methods", json!(METHODS)),
            ("features", json!(["catalog-journal-v1"])),
        ]))
    }

    fn runtime_capabilities(&mut self, socket: &Path) -> Value {
        let control = socket.with_extension("ctl");
        let Ok(metadata) = fs::metadata(&control) else {
            return json!({"capabilities": [], "compatibility": "legacy-replay"});
        };
        let key = (
            metadata.dev(),
            metadata.ino(),
            metadata.mtime() * 1_000_000_000 + metadata.mtime_nsec(),
        );
        let name = control.to_string_lossy().into_owned();
        if let Some((cached, result)) = self.capabilities.get(&name) {
            if *cached == key {
                return result.clone();
            }
        }
        let advertised = (|| -> Result<Value> {
            let mut connection = UnixStream::connect(&control).map_err(os)?;
            connection
                .set_read_timeout(Some(Duration::from_secs(1)))
                .map_err(os)?;
            connection
                .set_write_timeout(Some(Duration::from_secs(1)))
                .map_err(os)?;
            connection.write_all(b"CAPS\n").map_err(os)?;
            receive(&mut connection)
        })();
        let result = match advertised {
            Ok(mut value)
                if value["protocol"].as_i64() == Some(1) && value["capabilities"].is_array() =>
            {
                value["compatibility"] = json!("advertised");
                value
            }
            _ => json!({"capabilities": null, "compatibility": "unadvertised"}),
        };
        self.capabilities.insert(name, (key, result.clone()));
        result
    }

    fn reap(&mut self) {
        // Reap exited children; never restart them.
        self.children
            .retain_mut(|(_, child)| matches!(child.try_wait(), Ok(None)));
    }

    fn terminal_path(&self, data: &Value, identity: &str) -> Result<PathBuf> {
        match find_terminal(data, identity) {
            Some((_, terminal)) => catalog::socket_path(&self.directory, id(terminal)),
            None => fail("Terminal no longer exists"),
        }
    }

    fn dispatch(&mut self, message: &Value) -> Result<Map<String, Value>> {
        if message["protocol"].as_i64() != Some(PROTOCOL) {
            return fail("Incompatible session service protocol");
        }
        let method = message["method"].as_str().unwrap_or("");
        if !METHODS.contains(&method) {
            return fail("Unknown session service method");
        }
        let text = |key: &str| -> Result<String> {
            match message.get(key) {
                None => Ok(String::new()),
                Some(Value::String(value)) if value.chars().count() <= 1024 => Ok(value.clone()),
                Some(_) => fail("Invalid session service parameters"),
            }
        };
        let project_directory = match message.get("project_directory") {
            None | Some(Value::Null) => None,
            Some(Value::String(value)) if value.len() <= 1023 => Some(value.clone()),
            Some(_) => return fail("Invalid workspace folder"),
        };
        let (identity, title, destination) =
            (text("identity")?, text("title")?, text("destination")?);
        let position = match message.get("position") {
            None => -1,
            Some(value) => match value.as_i64() {
                Some(position) => position,
                None => return fail("Invalid session service parameters"),
            },
        };
        match method {
            "service.retire" => {
                // Runtime authority, independent of any client's window model. A
                // stale observer must never retire a replacement service instance.
                if message["expected_instance"].as_str() != Some(self.instance.as_str()) {
                    return fail("Session service changed; inspect its status before retrying");
                }
                self.running = false;
                return Ok(object([("retired", json!(true))]));
            }
            "service.shutdown" => {
                self.running = false;
                return Ok(object([("stopped", json!(true))]));
            }
            "health" => return self.health(),
            "catalog.read" => {
                // Read committed metadata only: no journal reconciliation, seeding,
                // terminal probing, recovery, ownership changes or catalog rewrite.
                let _lock = catalog::locked(&self.directory)?;
                if !self.directory.join("workspaces.json").exists() {
                    return fail("Saved workspace catalog is unavailable");
                }
                return Ok(object([("data", catalog::load(&self.directory)?)]));
            }
            "terminal.status" | "terminal.lifecycle" => {
                // Read one runtime fact without attachment or capability enumeration.
                let lock = catalog::locked(&self.directory)?;
                let data = catalog::load(&self.directory)?;
                let path = self.terminal_path(&data, &identity)?;
                let state = match catalog::owned_sockets(
                    &path,
                    "Session endpoint is not a user-owned socket",
                ) {
                    Ok(()) => Self::runtime_state(&path),
                    Err(_) => "unavailable",
                };
                drop(lock);
                let mut reply = object([("id", json!(identity)), ("state", json!(state))]);
                if method == "terminal.lifecycle" {
                    reply.insert("lifecycle".into(), lifecycle::detail(&path, state));
                }
                return Ok(reply);
            }
            _ => {}
        }
        if message["runtime"].as_str() != Some(&*self.launch.runtime.to_string_lossy())
            || message["cwd"].as_str() != Some(&*self.launch.cwd.to_string_lossy())
        {
            return fail("Session service configuration differs; stop the service before changing runtime or directory");
        }
        match method {
            "terminal.dismiss" => {
                // Metadata-only removal, serialized with recovery/create/close. Never
                // reconcile destructive journals or stop a process for this action.
                let _lock = catalog::locked(&self.directory)?;
                if !self.directory.join("workspaces.json").is_file() {
                    return fail("Saved workspace catalog is unavailable");
                }
                let mut data = catalog::load(&self.directory)?;
                if ["pending_create", "pending_stop", "pending_close"]
                    .iter()
                    .any(|key| data.get(*key).is_some())
                {
                    return fail("A session change is pending. Refresh before closing this pane.");
                }
                let path = self.terminal_path(&data, &identity)?;
                catalog::owned_sockets(
                    &path,
                    "Runtime status unavailable. Check again before closing this pane.",
                )?;
                if catalog::endpoint_alive(&path.with_extension("ctl"))?
                    || catalog::endpoint_alive(&path)?
                {
                    return fail("Terminal is running. Use Close terminal to confirm stopping it.");
                }
                for workspace in catalog::workspaces_mut(&mut data) {
                    if let Some(list) = workspace["terminals"].as_array_mut() {
                        list.retain(|terminal| id(terminal) != identity);
                    }
                }
                catalog::save(&self.directory, &data)?;
                return Ok(self.data_reply(data));
            }
            "initialize" => {
                // Existing identities without a live runtime are retained as stopped.
                // Only initial seeding or explicit recovery can restart saved shells.
                let lock = catalog::locked(&self.directory)?;
                if self.directory.join("workspaces.json").exists()
                    || self.directory.join("workspaces.tsv").exists()
                {
                    let mut data = catalog::load(&self.directory)?;
                    self.reconcile(&mut data)?;
                    catalog::save(&self.directory, &data)?;
                    return Ok(self.data_reply(data));
                }
                drop(lock);
            }
            "session.stop" => {
                let _lock = catalog::locked(&self.directory)?;
                let mut data = catalog::load(&self.directory)?;
                self.reconcile(&mut data)?;
                let targets = message.get("terminal_ids").cloned().unwrap_or(Value::Null);
                catalog::begin_stop(&self.directory, &self.launch.runtime, &mut data, &targets)?;
                return Ok(self.data_reply(data));
            }
            "terminal.recover" => {
                let _lock = catalog::locked(&self.directory)?;
                let mut data = catalog::load(&self.directory)?;
                self.reconcile(&mut data)?;
                let Some((workspace, terminal)) = find_terminal(&data, &identity) else {
                    return fail("Terminal no longer exists");
                };
                let path = catalog::socket_path(&self.directory, id(terminal))?;
                if catalog::alive(&path)? {
                    return fail("Terminal is still running; recovery would replace a live shell");
                }
                let mut single = workspace.clone();
                single["terminals"] = json!([terminal]);
                let subset = json!({"workspaces": [single]});
                let folder = workspace
                    .get("directory")
                    .and_then(Value::as_str)
                    .map_or_else(
                        || self.launch.cwd.to_string_lossy().into_owned(),
                        str::to_owned,
                    );
                catalog::directory_value(&folder, Some(&self.launch.cwd), true)?;
                catalog::recover_sessions(&self.directory, &subset)?;
                catalog::ensure_sessions(
                    &self.directory,
                    &subset,
                    &self.launch,
                    &mut self.children,
                )?;
                return Ok(self.data_reply(data));
            }
            _ => {}
        }
        let data = catalog::manage(
            &self.directory,
            &self.launch,
            method,
            &identity,
            &title,
            &destination,
            position,
            project_directory.as_deref(),
            &mut self.children,
        )?;
        Ok(self.data_reply(data))
    }
}

fn wait_readable(listener: &UnixListener) -> io::Result<bool> {
    let mut descriptor = libc::pollfd {
        fd: listener.as_raw_fd(),
        events: libc::POLLIN,
        revents: 0,
    };
    // SAFETY: one valid pollfd for the duration of the call.
    match unsafe { libc::poll(&mut descriptor, 1, 1000) } {
        -1 => {
            let error = io::Error::last_os_error();
            if error.kind() == io::ErrorKind::Interrupted {
                Ok(false)
            } else {
                Err(error)
            }
        }
        count => Ok(count > 0),
    }
}

/// Serve the session authority until shutdown, retirement or a signal.
pub fn serve(
    directory: PathBuf,
    runtime: PathBuf,
    cwd: PathBuf,
    agent_bin: Option<PathBuf>,
) -> std::result::Result<(), String> {
    prepare_directory(&directory)?;
    let path = endpoint(&directory)?;
    let lock = catalog::lock_file(&directory.join("service.lock"), true).map_err(os)?;
    match fs::symlink_metadata(&path) {
        Ok(metadata) => {
            if !metadata.file_type().is_socket() || metadata.uid() != catalog::uid() {
                return fail("Refusing to replace a non-socket or unowned service endpoint");
            }
            match UnixStream::connect(&path) {
                Ok(_) => return fail("Service endpoint is already listening"),
                Err(error)
                    if matches!(
                        error.raw_os_error(),
                        Some(libc::ENOENT) | Some(libc::ECONNREFUSED)
                    ) => {}
                Err(error) => return Err(os(error)),
            }
            fs::remove_file(&path).map_err(os)?;
        }
        Err(error) if error.kind() == io::ErrorKind::NotFound => {}
        Err(error) => return Err(os(error)),
    }
    let mut authority = Authority {
        directory: directory.clone(),
        launch: Launch {
            runtime,
            cwd,
            agent_bin,
        },
        instance: catalog::random_hex(16)?,
        children: Vec::new(),
        capabilities: HashMap::new(),
        running: true,
    };
    // SAFETY: the handler only stores to an atomic flag.
    unsafe {
        libc::signal(
            libc::SIGTERM,
            interrupted as *const () as libc::sighandler_t,
        );
        libc::signal(libc::SIGINT, interrupted as *const () as libc::sighandler_t);
    }
    let mut listener = Some(UnixListener::bind(&path).map_err(os)?);
    fs::set_permissions(&path, fs::Permissions::from_mode(0o600)).map_err(os)?;
    let socket_inode = fs::metadata(&path).map_err(os)?.ino();
    println!("Session service {} listening at {}", pid(), path.display());
    let _ = io::stdout().flush();
    // Shutdown acknowledgement is a barrier: nobody may connect to a listener
    // that will never accept again. Retire before replying, then release
    // ownership so an immediate reopen can start a service.
    let retire = |listener: &mut Option<UnixListener>| {
        if listener.take().is_none() {
            return;
        }
        if fs::symlink_metadata(&path).is_ok_and(|metadata| metadata.ino() == socket_inode) {
            let _ = fs::remove_file(&path);
        }
        // SAFETY: the lock descriptor stays open until this function returns.
        unsafe { libc::flock(lock.as_raw_fd(), libc::LOCK_UN) };
    };
    let result = (|| -> std::result::Result<(), String> {
        while authority.running && RUNNING.load(Ordering::SeqCst) && directory.exists() {
            authority.reap();
            let Some(active) = listener.as_ref() else {
                break;
            };
            if !wait_readable(active).map_err(os)? {
                continue;
            }
            let mut connection = match active.accept() {
                Ok((connection, _)) => connection,
                Err(error) if error.kind() == io::ErrorKind::Interrupted => continue,
                Err(error) => return Err(os(error)),
            };
            let _ = connection.set_read_timeout(Some(Duration::from_secs(2)));
            let _ = connection.set_write_timeout(Some(Duration::from_secs(2)));
            let reply = receive(&mut connection).and_then(|message| authority.dispatch(&message));
            let mut body = object([("protocol", json!(PROTOCOL))]);
            match reply {
                Ok(result) => {
                    if !authority.running {
                        retire(&mut listener);
                    }
                    body.insert("ok".into(), json!(true));
                    body.extend(result);
                }
                Err(error) => {
                    body.insert("ok".into(), json!(false));
                    body.insert(
                        "error".into(),
                        json!(error.chars().take(1000).collect::<String>()),
                    );
                }
            }
            let _ = transmit(&mut connection, &Value::Object(body));
        }
        Ok(())
    })();
    // Only our endpoint is removed. Runtimes and their sockets survive.
    retire(&mut listener);
    result
}
