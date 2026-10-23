//! Linux app-menu entry for an unpacked release:
//! `band-session desktop-entry install|remove [--container NAME]`.
//! Only an entry this command wrote is ever replaced or removed.
use std::ffi::OsString;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};

type Result<T> = std::result::Result<T, String>;

pub const APP_ID: &str = "io.github.eelcoh.Band";
const MARKER: &str = "X-Band-Managed=true";

/// Desktop Exec quoting, then desktop string escaping. Not a shell command;
/// `%%` is a literal percent for field-code expansion.
fn exec_argument(value: &str) -> Result<String> {
    if value.chars().any(|ch| (ch as u32) < 32 || ch as u32 == 127) {
        return Err("Desktop launch paths cannot contain control characters".into());
    }
    let mut quoted = value.replace('%', "%%");
    for character in ['\\', '"', '`', '$'] {
        quoted = quoted.replace(character, &format!("\\{character}"));
    }
    Ok(format!("\"{}\"", quoted.replace('\\', "\\\\")))
}

fn entry(launcher: &Path, container: Option<&str>) -> Result<String> {
    let launcher = launcher.to_string_lossy();
    // Keep the Exec program fixed; the launcher is an argument, never sh -c.
    let mut command: Vec<&str> = match container {
        Some(name) => vec!["distrobox", "enter", name, "--"],
        None => vec!["/bin/sh"],
    };
    command.push(&launcher);
    let exec = command
        .iter()
        .map(|value| exec_argument(value))
        .collect::<Result<Vec<_>>>()?
        .join(" ");
    let try_exec = match container {
        Some(_) => "distrobox".to_owned(),
        None => launcher.replace('\\', "\\\\"),
    };
    Ok([
        "[Desktop Entry]",
        "Type=Application",
        "Version=1.0",
        "Name=Band",
        "Comment=Terminal with scrolling workspaces",
        &format!("Exec={exec}"),
        &format!("TryExec={try_exec}"),
        "Icon=utilities-terminal",
        "Terminal=false",
        "Categories=System;TerminalEmulator;",
        &format!("StartupWMClass={APP_ID}"),
        "StartupNotify=false",
        MARKER,
        "",
    ]
    .join("\n"))
}

fn desktop_path() -> Result<PathBuf> {
    let base = match std::env::var_os("XDG_DATA_HOME").filter(|value| !value.is_empty()) {
        Some(value) => PathBuf::from(value),
        None => {
            PathBuf::from(std::env::var_os("HOME").ok_or("HOME is not set")?).join(".local/share")
        }
    };
    if !base.is_absolute() {
        return Err("XDG_DATA_HOME must be an absolute path".into());
    }
    Ok(base.join("applications").join(format!("{APP_ID}.desktop")))
}

fn managed(path: &Path) -> Result<bool> {
    match fs::symlink_metadata(path) {
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(false),
        Err(error) => Err(error.to_string()),
        Ok(metadata) if !metadata.is_file() => Err(format!(
            "Refusing to replace an unmanaged desktop entry: {}",
            path.display()
        )),
        Ok(_) => {
            let text = fs::read_to_string(path).map_err(|error| error.to_string())?;
            if text.lines().any(|line| line == MARKER) {
                Ok(true)
            } else {
                Err(format!(
                    "Refusing to replace an unmanaged desktop entry: {}",
                    path.display()
                ))
            }
        }
    }
}

fn install(path: &Path, launcher: &Path, container: Option<&str>) -> Result<()> {
    let contents = entry(launcher, container)?;
    let directory = path.parent().ok_or("Invalid desktop entry path")?;
    fs::create_dir_all(directory).map_err(|error| error.to_string())?;
    managed(path)?;
    let temporary = directory.join(format!(".{APP_ID}.{}.tmp", std::process::id()));
    let result = fs::File::create(&temporary)
        .and_then(|mut file| file.write_all(contents.as_bytes()))
        .and_then(|()| fs::rename(&temporary, path));
    let _ = fs::remove_file(&temporary);
    result.map_err(|error| error.to_string())
}

fn remove(path: &Path) -> Result<bool> {
    if !managed(path)? {
        return Ok(false);
    }
    fs::remove_file(path).map_err(|error| error.to_string())?;
    Ok(true)
}

/// The `band` launcher beside this binary, as unpacked from the release.
fn launcher() -> Result<PathBuf> {
    let executable = std::env::current_exe().map_err(|error| error.to_string())?;
    let launcher = executable.with_file_name("band");
    if !launcher.is_file() {
        return Err(format!("Launcher not found: {}", launcher.display()));
    }
    Ok(launcher)
}

pub fn command(mut args: impl Iterator<Item = OsString>) -> i32 {
    const USAGE: &str = "usage: band-session desktop-entry install [--container NAME] | remove";
    let action = args.next();
    let mut container = None;
    while let Some(flag) = args.next() {
        match (flag.to_str(), args.next()) {
            (Some("--container"), Some(name)) if !name.is_empty() => {
                container = Some(name.to_string_lossy().into_owned())
            }
            _ => {
                eprintln!("{USAGE}");
                return 2;
            }
        }
    }
    let result = match action.as_ref().and_then(|value| value.to_str()) {
        Some("install") => desktop_path().and_then(|path| {
            install(&path, &launcher()?, container.as_deref())?;
            Ok(format!("App menu entry installed: {}", path.display()))
        }),
        Some("remove") if container.is_none() => desktop_path().and_then(|path| {
            Ok(if remove(&path)? {
                format!("App menu entry removed: {}", path.display())
            } else {
                "No app menu entry was installed".into()
            })
        }),
        _ => {
            eprintln!("{USAGE}");
            return 2;
        }
    };
    match result {
        Ok(message) => {
            println!("{message}");
            0
        }
        Err(error) => {
            eprintln!("Band: {error}");
            1
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn exec_lines_quote_paths_and_keep_the_program_fixed() {
        let launcher = Path::new("/opt/space ü/100%/$HOME/\"q\"/band");
        let text = entry(launcher, None).unwrap();
        assert!(text.contains(r#"Exec="/bin/sh" "/opt/space ü/100%%/\\$HOME/\\"q\\"/band""#));
        assert!(text.lines().any(|line| line == MARKER));
        let boxed = entry(Path::new("/home/me/band/bin/band"), Some("build-machine")).unwrap();
        assert!(boxed
            .contains(r#"Exec="distrobox" "enter" "build-machine" "--" "/home/me/band/bin/band""#));
        assert!(boxed.contains("TryExec=distrobox"));
        assert!(entry(Path::new("/bad\npath"), None).is_err());
    }

    #[test]
    fn only_managed_entries_are_replaced_or_removed() {
        let root = std::env::temp_dir().join(format!("wb-desktop-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let path = root.join("applications").join(format!("{APP_ID}.desktop"));
        let launcher = Path::new("/opt/band/bin/band");
        install(&path, launcher, None).unwrap();
        install(&path, launcher, Some("box")).unwrap();
        assert!(fs::read_to_string(&path).unwrap().contains("distrobox"));
        assert!(remove(&path).unwrap());
        assert!(!remove(&path).unwrap());
        fs::write(&path, "[Desktop Entry]\nName=Mine\n").unwrap();
        assert!(install(&path, launcher, None).is_err());
        assert!(remove(&path).is_err());
        assert_eq!(
            fs::read_to_string(&path).unwrap(),
            "[Desktop Entry]\nName=Mine\n"
        );
        fs::remove_file(&path).unwrap();
        std::os::unix::fs::symlink("/etc/hostname", &path).unwrap();
        assert!(install(&path, launcher, None).is_err());
        fs::remove_dir_all(root).unwrap();
    }
}
