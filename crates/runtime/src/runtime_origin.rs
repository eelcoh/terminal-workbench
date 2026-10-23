//! Immutable launch paths for diagnostics across service replacement.
use std::path::Path;

fn path_json(path: Option<&Path>) -> String {
    path.filter(|path| path.is_absolute())
        .and_then(Path::to_str)
        // Do not truncate an identity: unavailable is safer than a different path.
        .filter(|text| text.len() <= 4096)
        .map_or_else(|| "null".to_owned(), crate::context::json_string)
}

pub(crate) fn capture() -> String {
    let runtime = std::env::current_exe().ok();
    let helpers = std::env::var_os("BAND_AGENT_BIN").map(std::path::PathBuf::from);
    format!(
        "{{\"version\":1,\"runtime_binary\":{},\"agent_bin\":{}}}",
        path_json(runtime.as_deref()),
        path_json(helpers.as_deref())
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn origin_paths_preserve_identity_or_report_unavailable() {
        assert_eq!(path_json(None), "null");
        assert_eq!(path_json(Some(Path::new("relative"))), "null");
        assert_eq!(
            path_json(Some(Path::new(&format!("/{}", "x".repeat(4096))))),
            "null"
        );
        assert_eq!(
            path_json(Some(Path::new("/build \"é\"/line\n\\bin"))),
            "\"/build \\\"é\\\"/line\\u000a\\\\bin\""
        );
        use std::os::unix::ffi::OsStrExt;
        assert_eq!(
            path_json(Some(Path::new(std::ffi::OsStr::from_bytes(b"/bad\xff")))),
            "null"
        );
    }
}
