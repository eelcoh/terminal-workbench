use super::ProcessIdentity;
use std::path::PathBuf;
use std::process::Command;

pub fn identity(group: i32) -> Option<ProcessIdentity> {
    // Read the foreground leader only; never traverse process descendants.
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
    let executable = PathBuf::from(path);
    if executable
        .file_name()
        .and_then(|name| name.to_str())
        .is_some_and(|name| {
            matches!(name, "node" | "nodejs" | "python" | "python3") || name.starts_with("python3.")
        })
    {
        // KERN_PROCARGS2 starts with argc, executable, NUL padding, then argv.
        // A fixed prefix bounds per-sample work; oversized/unavailable arguments
        // leave launcher recognition unavailable rather than scanning processes.
        let mut bytes = [0u8; 16 * 1024];
        let mut size = bytes.len();
        let mut mib = [libc::CTL_KERN, libc::KERN_PROCARGS2, group];
        let result = unsafe {
            libc::sysctl(
                mib.as_mut_ptr(),
                mib.len() as u32,
                bytes.as_mut_ptr().cast(),
                &mut size,
                std::ptr::null_mut(),
                0,
            )
        };
        if let Some(identity) = (result == 0 && size <= bytes.len())
            .then(|| super::macos_arguments::decode(&bytes[..size]))
            .flatten()
        {
            return Some(identity);
        }
    }
    Some(ProcessIdentity {
        executable,
        script: None,
    })
}

pub fn working_directory(pid: i32) -> Option<PathBuf> {
    // A single process query, not lsof/ps or a process-tree walk.
    let mut info = std::mem::MaybeUninit::<libc::proc_vnodepathinfo>::zeroed();
    let size = std::mem::size_of::<libc::proc_vnodepathinfo>();
    let read = unsafe {
        libc::proc_pidinfo(
            pid,
            libc::PROC_PIDVNODEPATHINFO,
            0,
            info.as_mut_ptr().cast(),
            size as i32,
        )
    };
    if read != size as i32 {
        return None;
    }
    let info = unsafe { info.assume_init() };
    let bytes: Vec<u8> = info
        .pvi_cdir
        .vip_path
        .iter()
        .take_while(|byte| **byte != 0)
        .map(|byte| *byte as u8)
        .collect();
    if bytes.is_empty() || bytes.len() == info.pvi_cdir.vip_path.len() {
        return None;
    }
    use std::os::unix::ffi::OsStringExt;
    let path = PathBuf::from(std::ffi::OsString::from_vec(bytes));
    path.is_absolute().then_some(path)
}
