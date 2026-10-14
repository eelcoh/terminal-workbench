//! Unix process operations isolated from session organization.
use std::io;

pub fn stop_group(group: Option<i32>) -> io::Result<()> {
    if let Some(group) = group.filter(|group| *group > 1) {
        // The group comes directly from this session's controlling PTY.
        if unsafe { libc::killpg(group, libc::SIGKILL) } < 0 {
            let error = io::Error::last_os_error();
            if error.raw_os_error() != Some(libc::ESRCH) {
                return Err(error);
            }
        }
    }
    Ok(())
}

#[cfg(target_os = "linux")]
mod linux;
#[cfg(target_os = "macos")]
mod macos;
#[cfg(any(target_os = "macos", test))]
mod macos_arguments;

#[derive(Debug)]
pub struct ProcessIdentity {
    pub executable: std::path::PathBuf,
    pub script: Option<std::path::PathBuf>,
}

pub fn identity(group: Option<i32>) -> Option<ProcessIdentity> {
    let group = group.filter(|group| *group > 1)?;
    #[cfg(target_os = "linux")]
    {
        linux::identity(group)
    }
    #[cfg(target_os = "macos")]
    {
        macos::identity(group)
    }
    #[cfg(not(any(target_os = "linux", target_os = "macos")))]
    {
        let _ = group;
        None
    }
}

pub fn working_directory(pid: i32) -> Option<std::path::PathBuf> {
    if pid <= 1 {
        return None;
    }
    #[cfg(target_os = "linux")]
    {
        linux::working_directory(pid)
    }
    #[cfg(target_os = "macos")]
    {
        macos::working_directory(pid)
    }
    #[cfg(not(any(target_os = "linux", target_os = "macos")))]
    {
        let _ = pid;
        None
    }
}
