//! Decode only the executable and first launcher argument, never prompts/env.
use super::ProcessIdentity;
use std::os::unix::ffi::OsStrExt;
use std::path::PathBuf;

pub(super) fn decode(bytes: &[u8]) -> Option<ProcessIdentity> {
    let argc = i32::from_ne_bytes(bytes.get(..4)?.try_into().ok()?);
    if argc < 1 {
        return None;
    }
    let payload = bytes.get(4..)?;
    let executable_end = payload.iter().position(|byte| *byte == 0)?;
    if executable_end == 0 {
        return None;
    }
    let executable = PathBuf::from(std::ffi::OsStr::from_bytes(&payload[..executable_end]));
    let argv = payload.get(executable_end + 1..)?;
    let argv_start = argv.iter().position(|byte| *byte != 0)?;
    let argv = &argv[argv_start..];
    let first_end = argv.iter().position(|byte| *byte == 0)?;
    let invocation = Some(PathBuf::from(std::ffi::OsStr::from_bytes(
        &argv[..first_end],
    )));
    let script = if argc > 1 {
        let tail = argv.get(first_end + 1..)?;
        let end = tail.iter().position(|byte| *byte == 0)?;
        (end > 0).then(|| PathBuf::from(std::ffi::OsStr::from_bytes(&tail[..end])))
    } else {
        None
    };
    Some(ProcessIdentity {
        executable,
        script,
        invocation,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn reads_bounded_launcher_prefix_without_consuming_prompts_or_environment() {
        let mut bytes = 3i32.to_ne_bytes().to_vec();
        bytes.extend_from_slice(b"/opt/node\0\0\0node\0/pkg/@github/copilot/npm-loader.js\0private prompt\0SECRET=value\0");
        let identity = decode(&bytes).expect("valid prefix");
        assert_eq!(identity.executable, PathBuf::from("/opt/node"));
        assert_eq!(
            identity.script,
            Some(PathBuf::from("/pkg/@github/copilot/npm-loader.js"))
        );
        assert_eq!(identity.invocation, Some(PathBuf::from("node")));
        assert!(decode(&[]).is_none());
        assert!(decode(&0i32.to_ne_bytes()).is_none());
        assert!(decode(&bytes[..10]).is_none());
        let mut native = 1i32.to_ne_bytes().to_vec();
        native.extend_from_slice(b"/bin/copilot\0\0copilot\0");
        assert!(decode(&native).expect("native prefix").script.is_none());
    }
}
