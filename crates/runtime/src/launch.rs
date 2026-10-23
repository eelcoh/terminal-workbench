//! Open an installed Band window on the saved sessions: user state and
//! preferences, a leased window slot, the session service and the Qt viewer.
//! Closing the window detaches; shells and workspaces remain.
use crate::service::{expand_user, initialize, prepare_directory, resolve};
use serde_json::{json, Map, Value};
use std::ffi::OsString;
use std::fs::{self, DirBuilder, File, OpenOptions};
use std::io::{self, Read, Write};
use std::os::unix::fs::{DirBuilderExt, MetadataExt, OpenOptionsExt};
use std::os::unix::io::{AsRawFd, IntoRawFd};
use std::os::unix::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

type Result<T> = std::result::Result<T, String>;

fn os(error: io::Error) -> String {
    error.to_string()
}

fn env(name: &str) -> Option<String> {
    std::env::var(name).ok()
}

fn home() -> PathBuf {
    env("HOME").map_or_else(|| PathBuf::from("/"), PathBuf::from)
}

fn uid() -> u32 {
    // SAFETY: getuid has no preconditions and cannot fail.
    unsafe { libc::getuid() }
}

fn random_suffix() -> Result<String> {
    let mut bytes = [0; 6];
    File::open("/dev/urandom")
        .and_then(|mut file| file.read_exact(&mut bytes))
        .map_err(os)?;
    Ok(bytes.iter().map(|byte| format!("{byte:02x}")).collect())
}

/// User-owned state for installed windows, separate from checkout sessions.
pub fn session_directory() -> Result<PathBuf> {
    if let Some(explicit) = env("BAND_STATE_DIR").filter(|value| !value.is_empty()) {
        return Ok(resolve(&expand_user(&explicit)));
    }
    let base = match std::env::var_os("XDG_STATE_HOME") {
        Some(value) => expand_user(&value.to_string_lossy()),
        None => home().join(".local/state"),
    };
    if !base.is_absolute() {
        return Err("XDG_STATE_HOME must be an absolute path".into());
    }
    let directory = base.join("band");
    adopt_previous_state(&base.join("terminal-workbench/qt-preview"), &directory)?;
    Ok(resolve(&directory))
}

/// Running shells keep their sockets under the state directory used before the
/// rename to Band, so the new name points there instead of moving anything.
fn adopt_previous_state(previous: &Path, directory: &Path) -> Result<()> {
    if directory.symlink_metadata().is_ok() || !previous.is_dir() {
        return Ok(());
    }
    create_private(directory.parent().unwrap_or(Path::new(".")))?;
    match std::os::unix::fs::symlink(previous, directory) {
        Err(error) if error.kind() != io::ErrorKind::AlreadyExists => Err(os(error)),
        _ => Ok(()),
    }
}

pub fn config_directory() -> PathBuf {
    if let Some(explicit) = env("BAND_CONFIG_DIR").filter(|value| !value.is_empty()) {
        let path = expand_user(&explicit);
        return if path.is_absolute() {
            path
        } else {
            std::env::current_dir().unwrap_or_default().join(path)
        };
    }
    config_base().join("band")
}

fn config_base() -> PathBuf {
    env("XDG_CONFIG_HOME")
        .filter(|value| !value.is_empty())
        .map_or_else(|| home().join(".config"), PathBuf::from)
}

fn create_private(directory: &Path) -> Result<()> {
    DirBuilder::new()
        .recursive(true)
        .mode(0o700)
        .create(directory)
        .map_err(os)
}

/// Publish without replacing an existing file, even under a concurrent launch.
fn publish_once(destination: &Path, data: &[u8]) -> Result<()> {
    let parent = destination.parent().unwrap_or(Path::new("."));
    let temporary = parent.join(format!(".migrate-{}", random_suffix()?));
    let result = (|| -> io::Result<()> {
        let mut file = OpenOptions::new()
            .write(true)
            .create_new(true)
            .mode(0o600)
            .open(&temporary)?;
        file.write_all(data)?;
        file.sync_all()?;
        match fs::hard_link(&temporary, destination) {
            Err(error) if error.kind() == io::ErrorKind::AlreadyExists => Ok(()),
            other => other,
        }
    })();
    let _ = fs::remove_file(&temporary);
    result.map_err(os)
}

fn copy_missing(source: &Path, destination: &Path) -> Result<()> {
    // Never follow a destination symlink or overwrite an existing choice.
    if destination.is_symlink() {
        return Ok(());
    }
    if source.is_symlink() {
        if !destination.exists() {
            create_private(destination.parent().unwrap_or(Path::new(".")))?;
            match std::os::unix::fs::symlink(resolve(source), destination) {
                Err(error) if error.kind() != io::ErrorKind::AlreadyExists => {
                    return Err(os(error))
                }
                _ => {}
            }
        }
    } else if source.is_dir() {
        if destination.exists() && !destination.is_dir() {
            return Ok(());
        }
        create_private(destination)?;
        let mut children: Vec<_> = fs::read_dir(source)
            .map_err(os)?
            .filter_map(|entry| entry.ok().map(|entry| entry.file_name()))
            .collect();
        children.sort();
        for name in children {
            let text = name.to_string_lossy();
            if !text.starts_with('.') && !text.ends_with(".lock") {
                copy_missing(&source.join(&name), &destination.join(&name))?;
            }
        }
    } else if source.is_file() && !destination.exists() {
        create_private(destination.parent().unwrap_or(Path::new(".")))?;
        publish_once(destination, &fs::read(source).map_err(os)?)?;
    }
    Ok(())
}

/// Preferences directory. Settings from before the rename to Band, and on macOS
/// from Application Support, are copied once; the originals stay untouched.
pub fn prepare_config_directory() -> Result<PathBuf> {
    let directory = config_directory();
    if env("BAND_CONFIG_DIR").is_some_and(|value| !value.is_empty()) {
        return Ok(directory);
    }
    migrate_previous_config(&config_base().join("terminal-workbench"), &directory)?;
    if cfg!(target_os = "macos") {
        migrate_macos_config(&home(), &directory)?;
    }
    Ok(directory)
}

fn migrate_previous_config(previous: &Path, directory: &Path) -> Result<()> {
    if directory.symlink_metadata().is_ok() || !previous.is_dir() {
        return Ok(());
    }
    copy_missing(previous, directory)?;
    // Hidden markers record earlier one-time migrations; keep them so those
    // never repeat. Lock files are transient and stay behind.
    for marker in [
        ".migrated-macos-config-v1",
        "ui/linux/.migrated-v1",
        "ui/macos/.migrated-v1",
    ] {
        copy_missing(&previous.join(marker), &directory.join(marker))?;
    }
    Ok(())
}

fn migrate_macos_config(home: &Path, directory: &Path) -> Result<()> {
    let legacy = home.join("Library/Application Support/Terminal Workbench");
    let marker = directory.join(".migrated-macos-config-v1");
    if marker.exists() || !legacy.is_dir() || resolve(directory) == resolve(&legacy) {
        return Ok(());
    }
    create_private(directory)?;
    copy_missing(&legacy.join("config.conf"), &directory.join("config.conf"))?;
    copy_missing(&legacy.join("themes"), &directory.join("themes"))?;
    for name in [
        "shortcuts.conf",
        "appearance.state",
        "appearance.conf",
        "terminal-font.json",
        ".migrated-v1",
    ] {
        copy_missing(
            &legacy.join("ui/macos").join(name),
            &directory.join("ui/macos").join(name),
        )?;
    }
    publish_once(
        &marker,
        b"Application Support preferences copied; original files retained.\n",
    )
}

const CONFIG_EXAMPLE: &str = include_str!("config-example.conf");

/// Write the example only when no config file (or symlink to one) exists.
fn init_config(path: &Path) -> Result<bool> {
    create_private(path.parent().unwrap_or(Path::new(".")))?;
    match OpenOptions::new().write(true).create_new(true).open(path) {
        Ok(mut file) => file.write_all(CONFIG_EXAMPLE.as_bytes()).map_err(os)?,
        Err(error) if error.kind() == io::ErrorKind::AlreadyExists => return Ok(false),
        Err(error) => return Err(os(error)),
    }
    Ok(true)
}

/// `band-session config-path [--prepare | --init]`: print the portable
/// config file. `--prepare` first copies earlier macOS preferences once;
/// `--init` also writes the example when no config file exists.
pub fn config_path(mut args: impl Iterator<Item = OsString>) -> i32 {
    let (prepare, init) = match (args.next(), args.next()) {
        (None, _) => (false, false),
        (Some(flag), None) if flag == "--prepare" => (true, false),
        (Some(flag), None) if flag == "--init" => (true, true),
        _ => {
            eprintln!("usage: band-session config-path [--prepare | --init]");
            return 2;
        }
    };
    let directory = if prepare {
        match prepare_config_directory() {
            Ok(directory) => directory,
            Err(error) => {
                eprintln!("Configuration: {error}");
                return 1;
            }
        }
    } else {
        config_directory()
    };
    let path = directory.join("config.conf");
    if init {
        match init_config(&path) {
            Ok(true) => {}
            Ok(false) => {
                println!("Existing configuration preserved: {}", path.display());
                return 0;
            }
            Err(error) => {
                eprintln!("Configuration: {error}");
                return 1;
            }
        }
    }
    println!("{}", path.display());
    0
}

fn private_regular(file: &File, message: &str, mask: u32) -> Result<()> {
    let metadata = file.metadata().map_err(os)?;
    if !metadata.is_file() || metadata.uid() != uid() || metadata.mode() & mask != 0 {
        return Err(message.into());
    }
    Ok(())
}

/// Lease the first free window slot for the viewer's lifetime: (lock, view path).
fn acquire_window_slot(views: &Path) -> Result<(File, PathBuf)> {
    let directory = views.join("windows");
    match DirBuilder::new().mode(0o700).create(&directory) {
        Err(error) if error.kind() != io::ErrorKind::AlreadyExists => return Err(os(error)),
        _ => {}
    }
    let metadata = fs::symlink_metadata(&directory).map_err(os)?;
    if !metadata.is_dir() || metadata.uid() != uid() || metadata.mode() & 0o022 != 0 {
        return Err("Window state directory must be user-owned and private".into());
    }
    for slot in 1..=64 {
        let lock = OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .mode(0o600)
            .custom_flags(libc::O_NOFOLLOW)
            .open(directory.join(format!("{slot}.lock")))
            .map_err(os)?;
        private_regular(&lock, "Unsafe window slot lock", 0)?;
        // SAFETY: the descriptor is open for the lifetime of `lock`.
        if unsafe { libc::flock(lock.as_raw_fd(), libc::LOCK_EX | libc::LOCK_NB) } == 0 {
            return Ok((lock, directory.join(format!("{slot}.view"))));
        }
        let error = io::Error::last_os_error();
        if error.raw_os_error() != Some(libc::EWOULDBLOCK) {
            return Err(os(error));
        }
    }
    Err("All 64 window slots are in use".into())
}

fn platform() -> &'static str {
    if cfg!(target_os = "macos") {
        "macos"
    } else {
        "linux"
    }
}

/// Client build identity; never inspects terminal or workspace data.
fn build_info(payload: &Path) -> Value {
    let mut result = Map::new();
    result.insert("kind".into(), json!("installed"));
    result.insert("payload_root".into(), json!(payload.to_string_lossy()));
    let path = payload.join("build-info.json");
    if !path.exists() {
        result.insert("metadata_error".into(), json!("Build metadata is missing"));
        return Value::Object(result);
    }
    let metadata = fs::read_to_string(&path)
        .map_err(os)
        .and_then(|text| serde_json::from_str::<Value>(&text).map_err(|error| error.to_string()))
        .and_then(|value| {
            if value["format"].as_i64() != Some(1) {
                return Err("Unsupported build metadata".into());
            }
            let revision = value["source"].as_str().unwrap_or("");
            if revision.len() != 40
                || !revision
                    .bytes()
                    .all(|byte| matches!(byte, b'0'..=b'9' | b'a'..=b'f'))
            {
                return Err("Invalid build source revision".into());
            }
            Ok(value)
        });
    match metadata {
        Ok(value) => {
            result.insert("revision".into(), value["source"].clone());
            result.insert("version".into(), value["version"].clone());
            for key in ["source_dirty", "qt_bundled", "selective_qml"] {
                result.insert(key.into(), json!(value[key] == json!(true)));
            }
        }
        Err(error) => {
            result.insert("metadata_error".into(), json!(error));
        }
    }
    Value::Object(result)
}

fn configuration(
    state: &Path,
    runtime: &Path,
    cwd: &Path,
    catalog: Value,
    preferences: &Path,
    payload: &Path,
) -> Map<String, Value> {
    let text = |path: PathBuf| json!(path.to_string_lossy());
    let ui = preferences.join("ui").join(platform());
    let mut settings = Map::new();
    for (key, value) in [
        ("state", text(state.into())),
        ("runtime", text(runtime.into())),
        ("cwd", text(cwd.into())),
        ("catalog", catalog),
        ("build_info", build_info(payload)),
        ("preferences", text(preferences.join("config.conf"))),
        ("font_overrides", text(ui.join("terminal-font.json"))),
        ("shortcut_overrides", text(ui.join("shortcuts.conf"))),
        ("theme_directory", text(preferences.join("themes"))),
        ("theme_store", text(preferences.join("themes/imports"))),
        ("saved_session", json!(true)),
        ("catalog_updates", json!(true)),
    ] {
        settings.insert(key.into(), value);
    }
    settings
}

fn utc_now() -> String {
    let seconds = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_or(0, |d| d.as_secs()) as i64;
    let (days, time) = (seconds.div_euclid(86_400), seconds.rem_euclid(86_400));
    // Civil date from days since 1970-01-01 (Howard Hinnant's algorithm).
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let day = doy - (153 * mp + 2) / 5 + 1;
    let month = if mp < 10 { mp + 3 } else { mp - 9 };
    let year = yoe + era * 400 + i64::from(month <= 2);
    format!(
        "{year:04}-{month:02}-{day:02}T{:02}:{:02}:{:02}+00:00",
        time / 3600,
        time % 3600 / 60,
        time % 60
    )
}

/// Send stdout/stderr to the private launch log for desktop starts.
fn start_log(state: &Path) -> Result<PathBuf> {
    prepare_directory(state)?;
    let path = state.join("launch.log");
    let log = OpenOptions::new()
        .append(true)
        .create(true)
        .mode(0o600)
        .custom_flags(libc::O_NOFOLLOW)
        .open(&path)
        .map_err(os)?;
    private_regular(
        &log,
        "Launch log must be a private regular file owned by you",
        0o077,
    )?;
    let _ = io::stdout().flush();
    // SAFETY: both descriptors are valid; dup2 replaces stdout and stderr.
    unsafe {
        libc::dup2(log.as_raw_fd(), 1);
        libc::dup2(log.as_raw_fd(), 2);
    }
    println!("\nBand launch: {}", utc_now());
    Ok(path)
}

fn show_error(message: &str) {
    if cfg!(target_os = "macos") {
        let script = "on run argv\n display alert \"Band could not start\" message (item 1 of argv) as critical\nend run";
        let _ = Command::new("/usr/bin/osascript")
            .args(["-e", script, message])
            .status();
    } else {
        let _ = Command::new("notify-send")
            .args(["--app-name=Band", "Band could not start", message])
            .status();
    }
}

fn inheritable(lock: &File) -> Result<()> {
    // SAFETY: the descriptor is open; clearing FD_CLOEXEC keeps it across exec.
    if unsafe { libc::fcntl(lock.as_raw_fd(), libc::F_SETFD, 0) } != 0 {
        return Err(os(io::Error::last_os_error()));
    }
    Ok(())
}

fn temporary_file(directory: &Path, prefix: &str, contents: &str) -> Result<PathBuf> {
    loop {
        let path = directory.join(format!("{prefix}{}.json", random_suffix()?));
        match OpenOptions::new()
            .write(true)
            .create_new(true)
            .mode(0o600)
            .open(&path)
        {
            Ok(mut file) => {
                file.write_all(contents.as_bytes()).map_err(os)?;
                return Ok(path);
            }
            Err(error) if error.kind() == io::ErrorKind::AlreadyExists => continue,
            Err(error) => return Err(os(error)),
        }
    }
}

fn saved_window(payload: &Path, viewer: &Path, preferences: &Path, desktop: bool) -> Result<i32> {
    let runtime = payload.join("bin/band-session");
    let runtime =
        fs::canonicalize(&runtime).map_err(|error| format!("{}: {error}", runtime.display()))?;
    let cwd = fs::canonicalize(home()).map_err(os)?;
    let state = session_directory()?;
    prepare_directory(&state)?;
    let views = state.join("qt");
    match DirBuilder::new().mode(0o700).create(&views) {
        Err(error) if error.kind() != io::ErrorKind::AlreadyExists => return Err(os(error)),
        _ => {}
    }
    let (lease, view) = acquire_window_slot(&views)?;
    let catalog = initialize(&state, &runtime, &cwd, &payload.join("bin"))?["data"].take();
    let mut settings = configuration(&state, &runtime, &cwd, catalog, preferences, payload);
    settings.insert(
        "view_state".into(),
        json!(view.with_extension("json").to_string_lossy()),
    );
    settings.insert("installed".into(), json!(true));
    settings.insert("viewer".into(), json!(viewer.to_string_lossy()));
    let contents = Value::Object(settings).to_string();
    if desktop {
        // Replace the launcher with Qt so the desktop owns the actual GUI
        // process. Its inherited lease lives until Qt exits; Qt removes this
        // private config after reading it, and exec failure removes it here.
        let config = temporary_file(&views, "launch-", &contents)?;
        inheritable(&lease)?;
        println!(
            "Opening Band; closing detaches. Session state: {}",
            state.display()
        );
        let _ = io::stdout().flush();
        let _ = lease.into_raw_fd();
        let error = Command::new(viewer)
            .arg(&config)
            .arg("--remove-launch-config")
            .exec();
        let _ = fs::remove_file(&config);
        return Err(format!("{}: {error}", viewer.display()));
    }
    let directory = PathBuf::from(format!("/tmp/wb-qt-window-{}", random_suffix()?));
    DirBuilder::new()
        .mode(0o700)
        .create(&directory)
        .map_err(os)?;
    let result = (|| -> Result<i32> {
        let config = directory.join("ui.json");
        fs::write(&config, &contents).map_err(os)?;
        println!(
            "Qt saved sessions: {}. Closing detaches; shells and workspaces remain.",
            state.display()
        );
        println!("Qt window view: {}", view.with_extension("json").display());
        inheritable(&lease)?;
        let status = Command::new(viewer).arg(&config).status().map_err(os)?;
        Ok(status.code().unwrap_or(1))
    })();
    let _ = fs::remove_dir_all(&directory);
    drop(lease);
    result
}

/// `band-session open --payload DIR --viewer PATH [--desktop]`.
pub fn open(mut args: impl Iterator<Item = OsString>) -> i32 {
    const USAGE: &str = "usage: band-session open --payload DIR --viewer PATH [--desktop]";
    let (mut payload, mut viewer, mut desktop) = (None, None, false);
    while let Some(flag) = args.next() {
        match flag.to_str() {
            Some("--payload") => payload = args.next().map(PathBuf::from),
            Some("--viewer") => viewer = args.next().map(PathBuf::from),
            Some("--desktop") => desktop = true,
            _ => {
                eprintln!("{USAGE}");
                return 2;
            }
        }
    }
    let (Some(payload), Some(viewer)) = (payload, viewer) else {
        eprintln!("{USAGE}");
        return 2;
    };
    let payload = resolve(&payload);
    let viewer = resolve(&viewer);
    if !desktop {
        return match prepare_config_directory()
            .and_then(|preferences| saved_window(&payload, &viewer, &preferences, false))
        {
            Ok(code) => code,
            Err(error) => {
                eprintln!("Band: {error}");
                1
            }
        };
    }
    let log = session_directory().and_then(|state| start_log(&state));
    let result = log.as_ref().map_err(Clone::clone).and_then(|_| {
        let preferences = prepare_config_directory()?;
        saved_window(&payload, &viewer, &preferences, true)
    });
    match result {
        Ok(code) => code,
        Err(error) => {
            eprintln!("Band: {error}");
            show_error(&match &log {
                Ok(path) => format!("Could not open Band. Details: {}", path.display()),
                Err(_) => "Could not create the private launch log. Check your Band state directory permissions.".into(),
            });
            1
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn timestamps_use_civil_utc_dates() {
        let text = utc_now();
        assert_eq!(text.len(), 25);
        assert!(text.ends_with("+00:00") && text.as_bytes()[10] == b'T');
        assert!(text.as_str() >= "2026-01-01");
    }

    #[test]
    fn build_info_reports_metadata_errors_without_failing() {
        let root = std::env::temp_dir().join(format!("wb-build-info-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        assert_eq!(
            build_info(&root)["metadata_error"],
            "Build metadata is missing"
        );
        fs::write(
            root.join("build-info.json"),
            r#"{"format":1,"source":"short"}"#,
        )
        .unwrap();
        assert_eq!(
            build_info(&root)["metadata_error"],
            "Invalid build source revision"
        );
        let revision = "0123456789abcdef0123456789abcdef01234567";
        fs::write(
            root.join("build-info.json"),
            json!({"format": 1, "source": revision, "qt_bundled": true}).to_string(),
        )
        .unwrap();
        let info = build_info(&root);
        assert_eq!(info["revision"], revision);
        assert_eq!(info["qt_bundled"], true);
        assert_eq!(info["source_dirty"], false);
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn macos_migration_copies_preferences_once_without_sessions() {
        let root = std::env::temp_dir().join(format!("wb-migrate-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let home = root.join("home");
        let legacy = home.join("Library/Application Support/Terminal Workbench");
        fs::create_dir_all(legacy.join("themes/imports")).unwrap();
        fs::create_dir_all(legacy.join("ui/macos")).unwrap();
        fs::write(legacy.join("config.conf"), "managed Mac settings").unwrap();
        fs::write(legacy.join("themes/miasma"), "background = #222222\n").unwrap();
        fs::write(legacy.join("themes/imports/imports.json"), "{}").unwrap();
        fs::write(legacy.join("themes/imports/imports.lock"), "stale").unwrap();
        fs::write(legacy.join("ui/macos/terminal-font.json"), "{}").unwrap();
        fs::write(legacy.join("workspaces.json"), "never copy sessions").unwrap();
        let theme = root.join("managed-theme");
        fs::write(&theme, "background = #111111\n").unwrap();
        std::os::unix::fs::symlink(&theme, legacy.join("themes/linked")).unwrap();
        let target = home.join(".config/band");
        migrate_macos_config(&home, &target).unwrap();
        assert_eq!(
            fs::read_to_string(target.join("config.conf")).unwrap(),
            "managed Mac settings"
        );
        assert!(target.join("themes/miasma").exists());
        assert!(target.join("ui/macos/terminal-font.json").exists());
        assert!(target.join("themes/linked").is_symlink());
        assert!(!target.join("workspaces.json").exists());
        assert!(!target.join("themes/imports/imports.lock").exists());
        // Deleted choices never remigrate; the original folder stays untouched.
        fs::remove_file(target.join("themes/miasma")).unwrap();
        migrate_macos_config(&home, &target).unwrap();
        assert!(!target.join("themes/miasma").exists());
        assert!(legacy.join("themes/miasma").exists());
        // Existing choices and symlinked folders are never overwritten.
        let merged = root.join("merged");
        let existing = root.join("existing-themes");
        fs::create_dir_all(&merged).unwrap();
        fs::create_dir_all(&existing).unwrap();
        fs::write(merged.join("config.conf"), "new choice").unwrap();
        std::os::unix::fs::symlink(&existing, merged.join("themes")).unwrap();
        migrate_macos_config(&home, &merged).unwrap();
        assert_eq!(
            fs::read_to_string(merged.join("config.conf")).unwrap(),
            "new choice"
        );
        assert_eq!(fs::read_dir(&existing).unwrap().count(), 0);
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn config_init_preserves_existing_files_and_symlinks() {
        let root = std::env::temp_dir().join(format!("wb-config-init-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let fresh = root.join("fresh/config.conf");
        assert!(init_config(&fresh).unwrap());
        assert!(fs::read_to_string(&fresh)
            .unwrap()
            .starts_with("band-config-v1\n"));
        fs::write(&fresh, "# managed\n").unwrap();
        assert!(!init_config(&fresh).unwrap());
        assert_eq!(fs::read_to_string(&fresh).unwrap(), "# managed\n");
        let tracked = root.join("tracked");
        fs::write(&tracked, "# tracked\n").unwrap();
        let linked = root.join("linked/config.conf");
        fs::create_dir_all(linked.parent().unwrap()).unwrap();
        std::os::unix::fs::symlink(&tracked, &linked).unwrap();
        assert!(!init_config(&linked).unwrap());
        assert!(linked.is_symlink());
        assert_eq!(fs::read_to_string(&tracked).unwrap(), "# tracked\n");
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn previous_names_are_adopted_without_moving_anything() {
        let root = std::env::temp_dir().join(format!("wb-rename-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let previous = root.join("config/terminal-workbench");
        fs::create_dir_all(previous.join("ui/linux")).unwrap();
        fs::create_dir_all(previous.join("themes")).unwrap();
        fs::write(previous.join("config.conf"), "workbench-config-v1\n").unwrap();
        fs::write(previous.join("themes/mine"), "background = #000000\n").unwrap();
        fs::write(previous.join("ui/linux/.migrated-v1"), "").unwrap();
        fs::write(previous.join("ui/linux/shortcuts.lock"), "").unwrap();
        let directory = root.join("config/band");
        migrate_previous_config(&previous, &directory).unwrap();
        assert_eq!(
            fs::read_to_string(directory.join("config.conf")).unwrap(),
            "workbench-config-v1\n"
        );
        assert!(directory.join("themes/mine").exists());
        assert!(directory.join("ui/linux/.migrated-v1").exists());
        assert!(!directory.join("ui/linux/shortcuts.lock").exists());
        assert!(previous.join("config.conf").exists());
        // An existing Band directory is never refilled.
        fs::remove_file(directory.join("themes/mine")).unwrap();
        migrate_previous_config(&previous, &directory).unwrap();
        assert!(!directory.join("themes/mine").exists());
        // State is adopted in place so live sockets keep their paths.
        let state = root.join("state/terminal-workbench/qt-preview");
        fs::create_dir_all(&state).unwrap();
        let adopted = root.join("state/band");
        adopt_previous_state(&state, &adopted).unwrap();
        assert_eq!(resolve(&adopted), resolve(&state));
        let fresh = root.join("fresh/band");
        adopt_previous_state(&root.join("fresh/missing"), &fresh).unwrap();
        assert!(fresh.symlink_metadata().is_err());
        fs::remove_dir_all(root).unwrap();
    }
}
