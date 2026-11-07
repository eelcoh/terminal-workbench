//! Band-owned copies of an install's runtime and agent helpers.
//!
//! Installed windows run the session service, terminal runtimes and agent
//! launchers from `<state>/payloads/<version>-<hash>/`, not from the install,
//! so replacing or deleting Band.app (or a Linux folder) never takes helpers
//! away from running shells, and a new build gets a new runtime path, which
//! hands the session service over. Every runtime and service holds a shared
//! lock on its copy; a copy nobody holds is removed on the next open.
use crate::agent_hooks::AGENTS;
use std::fs::{self, DirBuilder, File, OpenOptions};
use std::io::{self, Read, Write};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt, PermissionsExt};
use std::os::unix::io::AsRawFd;
use std::path::{Path, PathBuf};

type Result<T> = std::result::Result<T, String>;

const LOCK: &str = ".in-use";
const RUNTIME: &str = "bin/band-session";

fn os(path: &Path) -> impl Fn(io::Error) -> String + '_ {
    move |error| format!("{}: {error}", path.display())
}

/// Files copied from the install, relative to its payload root, in a stable order.
fn sources(payload: &Path) -> Result<Vec<PathBuf>> {
    let mut found = vec![PathBuf::from(RUNTIME)];
    if payload.join("build-info.json").is_file() {
        found.push("build-info.json".into());
    }
    let mut pending = vec![PathBuf::from("integrations")];
    while let Some(relative) = pending.pop() {
        let directory = payload.join(&relative);
        let entries = match fs::read_dir(&directory) {
            Err(error) if error.kind() == io::ErrorKind::NotFound => continue,
            other => other.map_err(os(&directory))?,
        };
        for entry in entries {
            let entry = entry.map_err(os(&directory))?;
            let kind = entry.file_type().map_err(os(&entry.path()))?;
            let child = relative.join(entry.file_name());
            if kind.is_dir() {
                pending.push(child);
            } else if kind.is_file() {
                found.push(child);
            }
        }
    }
    found.sort();
    Ok(found)
}

/// FNV-1a over names and contents: a stable name for identical builds,
/// including uncommitted ones that share a revision.
fn digest(payload: &Path, files: &[PathBuf]) -> Result<u64> {
    let mut hash: u64 = 0xcbf2_9ce4_8422_2325;
    let mut feed = |bytes: &[u8]| {
        for byte in bytes {
            hash = (hash ^ u64::from(*byte)).wrapping_mul(0x0100_0000_01b3);
        }
    };
    let mut buffer = vec![0; 64 * 1024];
    for relative in files {
        feed(relative.to_string_lossy().as_bytes());
        feed(&[0]);
        let path = payload.join(relative);
        let mut file = File::open(&path).map_err(os(&path))?;
        loop {
            let read = file.read(&mut buffer).map_err(os(&path))?;
            if read == 0 {
                break;
            }
            feed(&buffer[..read]);
        }
        feed(&[0]);
    }
    Ok(hash)
}

fn version(payload: &Path) -> String {
    fs::read_to_string(payload.join("build-info.json"))
        .ok()
        .and_then(|text| serde_json::from_str::<serde_json::Value>(&text).ok())
        .and_then(|value| value["version"].as_str().map(str::to_owned))
        .filter(|text| {
            !text.is_empty()
                && text.len() <= 32
                && text
                    .bytes()
                    .all(|byte| byte.is_ascii_alphanumeric() || b".+-".contains(&byte))
        })
        .unwrap_or_else(|| "unknown".into())
}

fn lock(file: &File, operation: libc::c_int) -> bool {
    // SAFETY: the descriptor is open for the duration of the call.
    unsafe { libc::flock(file.as_raw_fd(), operation) == 0 }
}

fn open_lock(copy: &Path) -> io::Result<File> {
    OpenOptions::new()
        .read(true)
        .custom_flags(libc::O_NOFOLLOW)
        .open(copy.join(LOCK))
}

/// Plain byte copy: no extended attributes, so a quarantined install never
/// produces a quarantined runtime.
fn copy_file(from: &Path, to: &Path) -> Result<()> {
    let mut source = File::open(from).map_err(os(from))?;
    let mode = source.metadata().map_err(os(from))?.permissions().mode() & 0o755;
    let mut target = OpenOptions::new()
        .write(true)
        .create_new(true)
        .mode(mode | 0o600)
        .open(to)
        .map_err(os(to))?;
    io::copy(&mut source, &mut target).map_err(os(to))?;
    target
        .set_permissions(fs::Permissions::from_mode(mode | 0o600))
        .map_err(os(to))?;
    target.sync_all().map_err(os(to))
}

fn populate(payload: &Path, files: &[PathBuf], copy: &Path) -> Result<()> {
    for relative in files {
        let target = copy.join(relative);
        if let Some(parent) = target.parent() {
            DirBuilder::new()
                .recursive(true)
                .mode(0o700)
                .create(parent)
                .map_err(os(parent))?;
        }
        copy_file(&payload.join(relative), &target)?;
    }
    let bin = copy.join("bin");
    let names = AGENTS
        .iter()
        .map(|agent| format!("band-{agent}"))
        .chain(["band-agent-hook".to_owned()]);
    for name in names {
        let link = bin.join(name);
        std::os::unix::fs::symlink("band-session", &link).map_err(os(&link))?;
    }
    let lock = copy.join(LOCK);
    File::create(&lock)
        .and_then(|mut file| file.write_all(b"held by each runtime started from this copy\n"))
        .map_err(os(&lock))
}

fn random_suffix() -> Result<String> {
    let mut bytes = [0; 6];
    File::open("/dev/urandom")
        .and_then(|mut file| file.read_exact(&mut bytes))
        .map_err(|error| error.to_string())?;
    Ok(bytes.iter().map(|byte| format!("{byte:02x}")).collect())
}

/// A copy that a runtime, the service or an opening window still holds stays.
fn remove_unused(payloads: &Path, keep: &Path) {
    let Ok(entries) = fs::read_dir(payloads) else {
        return;
    };
    for entry in entries.flatten() {
        let path = entry.path();
        let name = entry.file_name();
        let name = name.to_string_lossy();
        if path == keep || !entry.file_type().is_ok_and(|kind| kind.is_dir()) {
            continue;
        }
        if name.starts_with(".copy-") || name.starts_with(".remove-") {
            // An interrupted copy or removal; a live copy is renamed within moments.
            let stale = entry
                .metadata()
                .and_then(|meta| meta.modified())
                .is_ok_and(|time| time.elapsed().is_ok_and(|age| age.as_secs() > 600));
            if stale {
                let _ = fs::remove_dir_all(&path);
            }
            continue;
        }
        let Ok(file) = open_lock(&path) else {
            continue;
        };
        if !lock(&file, libc::LOCK_EX | libc::LOCK_NB) {
            continue;
        }
        let Ok(suffix) = random_suffix() else {
            continue;
        };
        let doomed = payloads.join(format!(".remove-{suffix}"));
        if fs::rename(&path, &doomed).is_ok() {
            let _ = fs::remove_dir_all(&doomed);
        }
    }
}

/// The copy of `payload` this window runs from, and a lock that keeps it until
/// the session service holds it.
pub(crate) fn prepare(state: &Path, payload: &Path) -> Result<(PathBuf, File)> {
    let payloads = state.join("payloads");
    match DirBuilder::new().mode(0o700).create(&payloads) {
        Err(error) if error.kind() != io::ErrorKind::AlreadyExists => {
            return Err(os(&payloads)(error))
        }
        _ => {}
    }
    let files = sources(payload)?;
    let name = format!("{}-{:016x}", version(payload), digest(payload, &files)?);
    let copy = payloads.join(&name);
    let held = |copy: &Path| {
        open_lock(copy)
            .ok()
            .filter(|file| lock(file, libc::LOCK_SH | libc::LOCK_NB))
    };
    let file = match held(&copy) {
        Some(file) => file,
        None => {
            let staging = payloads.join(format!(".copy-{}", random_suffix()?));
            DirBuilder::new()
                .mode(0o700)
                .create(&staging)
                .map_err(os(&staging))?;
            let result = populate(payload, &files, &staging).and_then(|()| {
                match fs::rename(&staging, &copy) {
                    Ok(()) => Ok(()),
                    // Another window finished the same copy first.
                    Err(_) if copy.join(LOCK).is_file() => Ok(()),
                    Err(error) => Err(os(&copy)(error)),
                }
            });
            let _ = fs::remove_dir_all(&staging);
            result?;
            held(&copy).ok_or_else(|| format!("{}: copy is not usable", copy.display()))?
        }
    };
    remove_unused(&payloads, &copy);
    Ok((copy, file))
}

/// Keep this process's copy for as long as it runs. A runtime started from an
/// install or checkout has no lock file and holds nothing.
pub fn hold() {
    let Some(copy) = std::env::current_exe()
        .ok()
        .and_then(|exe| Some(exe.parent()?.parent()?.to_path_buf()))
    else {
        return;
    };
    if let Ok(file) = open_lock(&copy) {
        if lock(&file, libc::LOCK_SH) {
            // The descriptor is close-on-exec, so shells do not inherit it.
            std::mem::forget(file);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn install(root: &Path, runtime: &str) -> PathBuf {
        let payload = root.join("install");
        fs::create_dir_all(payload.join("bin")).unwrap();
        fs::create_dir_all(payload.join("integrations/copilot")).unwrap();
        fs::write(payload.join(RUNTIME), runtime).unwrap();
        fs::set_permissions(payload.join(RUNTIME), fs::Permissions::from_mode(0o755)).unwrap();
        fs::write(payload.join("integrations/copilot/hooks.json"), "{}").unwrap();
        fs::write(payload.join("build-info.json"), r#"{"version":"0.1.0"}"#).unwrap();
        payload
    }

    #[test]
    fn copies_are_named_by_content_and_removed_only_when_unheld() {
        let root = std::env::temp_dir().join(format!("wb-payload-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let state = root.join("state");
        fs::create_dir_all(&state).unwrap();
        let payload = install(&root, "first");
        let (first, guard) = prepare(&state, &payload).unwrap();
        assert!(first
            .file_name()
            .unwrap()
            .to_string_lossy()
            .starts_with("0.1.0-"));
        assert_eq!(fs::read_to_string(first.join(RUNTIME)).unwrap(), "first");
        assert_eq!(
            fs::metadata(first.join(RUNTIME))
                .unwrap()
                .permissions()
                .mode()
                & 0o777,
            0o755
        );
        assert!(first.join("integrations/copilot/hooks.json").is_file());
        for name in ["band-copilot", "band-agent-hook"] {
            assert_eq!(
                fs::read_link(first.join("bin").join(name)).unwrap(),
                Path::new("band-session")
            );
        }
        // The same build reuses its copy.
        let (again, _again) = prepare(&state, &payload).unwrap();
        assert_eq!(again, first);
        // A rebuild with the same version gets its own copy; a held copy stays.
        fs::write(payload.join(RUNTIME), "second").unwrap();
        let (second, _second) = prepare(&state, &payload).unwrap();
        assert_ne!(second, first);
        assert!(first.is_dir());
        // Once nothing holds the first copy, the next open removes it. A child
        // process started by another test can hold the lock between fork and
        // exec; then a later open removes it, so allow a few opens.
        drop(guard);
        drop(_again);
        for _ in 0..50 {
            prepare(&state, &payload).unwrap();
            if !first.exists() {
                break;
            }
            std::thread::sleep(std::time::Duration::from_millis(20));
        }
        assert!(!first.exists() && second.is_dir());
        // Deleting the install leaves the copy in use intact.
        fs::remove_dir_all(&payload).unwrap();
        assert_eq!(fs::read_to_string(second.join(RUNTIME)).unwrap(), "second");
        fs::remove_dir_all(root).unwrap();
    }
}
