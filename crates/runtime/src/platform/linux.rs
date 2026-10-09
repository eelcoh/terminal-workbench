use super::ProcessIdentity;
use std::fs::{self, File};
use std::io::Read;
use std::os::unix::ffi::OsStrExt;
use std::path::PathBuf;

pub fn identity(group: i32) -> Option<ProcessIdentity> {
    let executable = fs::read_link(format!("/proc/{group}/exe")).ok()?;
    let name = executable.file_name()?.to_str()?;
    let script = if name == "node" || name == "nodejs" || name.starts_with("python") {
        // Read a bounded launcher prefix; never log or expose user arguments.
        let mut bytes = Vec::new();
        File::open(format!("/proc/{group}/cmdline"))
            .ok()?
            .take(8192)
            .read_to_end(&mut bytes)
            .ok()?;
        bytes
            .split(|byte| *byte == 0)
            .nth(1)
            .filter(|arg| !arg.is_empty())
            .map(|arg| PathBuf::from(std::ffi::OsStr::from_bytes(arg)))
    } else {
        None
    };
    Some(ProcessIdentity { executable, script })
}
