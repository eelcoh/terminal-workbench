//! Decode libc's fixed 32-by-32 macOS vnode path buffer.
use std::os::unix::ffi::OsStringExt;
use std::path::PathBuf;

pub(super) fn decode(path: &[[libc::c_char; 32]; 32]) -> Option<PathBuf> {
    // libc models the contiguous MAXPATHLEN array as rows for older Rust versions.
    // Search the entire buffer for NUL; the outer length alone is not its capacity.
    let mut bytes: Vec<u8> = path.iter().flatten().map(|byte| *byte as u8).collect();
    let end = bytes.iter().position(|byte| *byte == 0)?;
    if end == 0 {
        return None;
    }
    bytes.truncate(end);
    let path = PathBuf::from(std::ffi::OsString::from_vec(bytes));
    path.is_absolute().then_some(path)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn buffer(bytes: &[u8]) -> [[libc::c_char; 32]; 32] {
        let mut path = [[0; 32]; 32];
        for (destination, byte) in path.iter_mut().flatten().zip(bytes) {
            *destination = *byte as libc::c_char;
        }
        path
    }

    #[test]
    fn reads_paths_across_rows_and_stops_at_nul() {
        let text = "/Users/test/Developer/github/projects/terminal-workbench/λ";
        assert_eq!(decode(&buffer(text.as_bytes())), Some(PathBuf::from(text)));
        let exactly_one_row = format!("/{}", "a".repeat(31));
        assert_eq!(
            decode(&buffer(exactly_one_row.as_bytes())),
            Some(PathBuf::from(exactly_one_row))
        );
        assert_eq!(
            decode(&buffer(b"/project\0ignored")),
            Some(PathBuf::from("/project"))
        );
        let maximum = format!("/{}", "a".repeat(1022));
        assert_eq!(
            decode(&buffer(maximum.as_bytes())),
            Some(PathBuf::from(maximum))
        );
        let non_utf8 = b"/project/\xff";
        assert_eq!(
            decode(&buffer(non_utf8)),
            Some(PathBuf::from(std::ffi::OsString::from_vec(
                non_utf8.to_vec()
            )))
        );
    }

    #[test]
    fn rejects_empty_relative_and_unterminated_paths() {
        assert!(decode(&buffer(b"")).is_none());
        assert!(decode(&buffer(b"relative/path")).is_none());
        let mut unterminated = [[b'a' as libc::c_char; 32]; 32];
        unterminated[0][0] = b'/' as libc::c_char;
        assert!(decode(&unterminated).is_none());
    }
}
