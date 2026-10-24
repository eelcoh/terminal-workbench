use super::ProcessIdentity;
use std::fs::{self, File};
use std::io::Read;
use std::os::unix::ffi::OsStrExt;
use std::path::PathBuf;

pub fn identity(group: i32) -> Option<ProcessIdentity> {
    let executable = fs::read_link(format!("/proc/{group}/exe")).ok()?;
    let name = executable.file_name()?.to_str()?;
    let mut invocation = None;
    let script = if name == "node" || name == "nodejs" || name.starts_with("python") {
        // Read a bounded launcher prefix; never log or expose user arguments.
        let mut bytes = Vec::new();
        File::open(format!("/proc/{group}/cmdline"))
            .ok()?
            .take(8192)
            .read_to_end(&mut bytes)
            .ok()?;
        let mut arguments = bytes.split(|byte| *byte == 0);
        invocation = arguments
            .next()
            .filter(|arg| !arg.is_empty())
            .map(|arg| PathBuf::from(std::ffi::OsStr::from_bytes(arg)));
        arguments
            .next()
            .filter(|arg| !arg.is_empty())
            .map(|arg| PathBuf::from(std::ffi::OsStr::from_bytes(arg)))
    } else {
        None
    };
    Some(ProcessIdentity {
        executable,
        script,
        invocation,
    })
}

pub fn working_directory(pid: i32) -> Option<PathBuf> {
    fs::read_link(format!("/proc/{pid}/cwd"))
        .ok()
        .filter(|path| path.is_absolute() && path.is_dir())
}

/// The login shell from the user database, for launchers (desktop entries,
/// containers) that supply no usable SHELL.
pub fn account_shell() -> Option<std::ffi::OsString> {
    use std::os::unix::ffi::OsStrExt;
    let entry = unsafe { libc::getpwuid(libc::getuid()) };
    if entry.is_null() || unsafe { (*entry).pw_shell }.is_null() {
        return None;
    }
    let shell = unsafe { std::ffi::CStr::from_ptr((*entry).pw_shell) }.to_bytes();
    (!shell.is_empty()).then(|| std::ffi::OsStr::from_bytes(shell).to_owned())
}
