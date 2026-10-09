use super::ProcessIdentity;
use std::path::PathBuf;
use std::process::Command;

pub fn identity(group: i32) -> Option<ProcessIdentity> {
    // Read only the executable path. macOS script-launcher discovery remains open.
    let output = Command::new("/bin/ps")
        .args(["-p", &group.to_string(), "-o", "comm="])
        .output()
        .ok()?;
    if !output.status.success() {
        return None;
    }
    let path = std::str::from_utf8(&output.stdout).ok()?.trim();
    if path.is_empty() {
        return None;
    }
    Some(ProcessIdentity {
        executable: PathBuf::from(path),
        script: None,
    })
}
