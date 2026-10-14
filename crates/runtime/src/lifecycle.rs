//! Bounded runtime-owned lifecycle evidence; written only at startup and shutdown.
use std::fs::{self, OpenOptions};
use std::io::{self, Write};
use std::os::unix::fs::{MetadataExt, OpenOptionsExt};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{SystemTime, UNIX_EPOCH};

static NEXT_FILE: AtomicU64 = AtomicU64::new(0);
fn quoted(text: &str) -> String {
    let mut value = String::from("\"");
    for ch in text.chars().take(80) {
        match ch {
            '\\' => value.push_str("\\\\"),
            '"' => value.push_str("\\\""),
            ch if ch.is_control() => value.push(' '),
            ch => value.push(ch),
        }
    }
    value.push('"');
    value
}
pub(crate) struct Lifecycle {
    path: PathBuf,
    shell_pid: Option<u32>,
    started: u128,
    finished: bool,
    running: bool,
}
impl Lifecycle {
    pub fn begin(socket: &Path) -> io::Result<Self> {
        let record = Self {
            path: socket.with_extension("lifecycle"),
            shell_pid: None,
            started: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map_or(0, |d| d.as_millis()),
            finished: false,
            running: false,
        };
        // Reset prior invocation evidence before attempting to start a new shell.
        record.write("starting", None)?;
        Ok(record)
    }
    pub fn running(&mut self, shell_pid: Option<u32>) {
        self.shell_pid = shell_pid;
        self.running = true;
        if let Err(error) = self.write("running", None) {
            eprintln!("Could not record running shell: {error}");
        }
    }
    pub fn finish(&mut self, cause: &'static str, status: &portable_pty::ExitStatus) {
        self.finished = true;
        if let Err(error) = self.write(cause, Some(status)) {
            eprintln!("Could not record shell exit: {error}");
        }
    }
    fn write(&self, cause: &str, status: Option<&portable_pty::ExitStatus>) -> io::Result<()> {
        let shell_pid = self
            .shell_pid
            .map_or_else(|| "null".to_owned(), |pid| pid.to_string());
        let code = status
            .filter(|s| s.signal().is_none())
            .map_or_else(|| "null".to_owned(), |s| s.exit_code().to_string());
        let signal = status
            .and_then(|s| s.signal())
            .map_or_else(|| "null".to_owned(), quoted);
        let bytes = format!("{{\"version\":1,\"runtime_pid\":{},\"shell_pid\":{},\"started_ms\":{},\"event\":{},\"exit_code\":{},\"signal\":{}}}\n",
            std::process::id(), shell_pid, self.started, quoted(cause), code, signal);
        match fs::symlink_metadata(&self.path) {
            Ok(metadata)
                if !metadata.file_type().is_file()
                    || metadata.uid() != unsafe { libc::geteuid() } =>
            {
                return Err(io::Error::other(
                    "lifecycle path is not a user-owned regular file",
                ));
            }
            Err(error) if error.kind() != io::ErrorKind::NotFound => return Err(error),
            _ => {}
        }
        let sequence = NEXT_FILE.fetch_add(1, Ordering::Relaxed);
        let temporary = self.path.with_extension(format!(
            "lifecycle.tmp-{}-{}-{}",
            std::process::id(),
            self.started,
            sequence
        ));
        let mut created = false;
        let result = (|| {
            let mut file = OpenOptions::new()
                .write(true)
                .create_new(true)
                .mode(0o600)
                .open(&temporary)?;
            created = true;
            file.write_all(bytes.as_bytes())?;
            file.sync_all()?;
            fs::rename(&temporary, &self.path)?;
            if let Some(parent) = self.path.parent() {
                // Persist the rename as well as its contents for service/reboot inspection.
                if let Err(error) =
                    fs::File::open(parent).and_then(|directory| directory.sync_all())
                {
                    // Some filesystems do not support syncing directory descriptors.
                    eprintln!("Could not sync lifecycle directory: {error}");
                }
            }
            Ok(())
        })();
        if result.is_err() && created {
            let _ = fs::remove_file(&temporary);
        }
        result
    }
}
impl Drop for Lifecycle {
    fn drop(&mut self) {
        if !self.finished {
            let event = if self.running {
                "runtime_error"
            } else {
                "startup_failed"
            };
            if let Err(error) = self.write(event, None) {
                eprintln!("Could not record runtime failure: {error}");
            }
        }
    }
}
