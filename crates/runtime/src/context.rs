//! Cached local process/Git context. File discovery runs off the PTY event thread.
use std::fs::{self, File};
use std::io::{self, Read};
use std::os::unix::ffi::{OsStrExt, OsStringExt};
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Context {
    pub cwd: String,
    pub repository: String,
    pub branch: String,
    pub worktree: String,
    pub detached: bool,
    pub remote: bool,
}

fn bounded(text: &str, maximum: usize) -> String {
    let mut end = text.len().min(maximum);
    while !text.is_char_boundary(end) {
        end -= 1;
    }
    text[..end].to_owned()
}
fn json_string(text: &str) -> String {
    let mut result = String::from("\"");
    for ch in text.chars() {
        match ch {
            '"' => result.push_str("\\\""),
            '\\' => result.push_str("\\\\"),
            ch if ch.is_control() => result.push_str(&format!("\\u{:04x}", ch as u32)),
            ch => result.push(ch),
        }
    }
    result.push('"');
    result
}
impl Context {
    pub fn json(&self) -> String {
        format!("{{\"available\":{},\"source\":\"{}\",\"cwd\":{},\"repository\":{},\"branch\":{},\"worktree\":{},\"detached\":{}}}\n",
            !self.cwd.is_empty(),if self.remote {"remote-unavailable"} else {"local-process"},
            json_string(&self.cwd),json_string(&self.repository),json_string(&self.branch),json_string(&self.worktree),self.detached)
    }
    pub fn payload(&self) -> Vec<u8> {
        let mut result = vec![
            1,
            u8::from(!self.cwd.is_empty())
                | (u8::from(self.detached) << 1)
                | (u8::from(self.remote) << 2),
        ];
        for field in [&self.cwd, &self.repository, &self.branch, &self.worktree] {
            result.extend_from_slice(&(field.len() as u16).to_be_bytes());
            result.extend_from_slice(field.as_bytes());
        }
        result
    }
}
fn read_small(path: &Path) -> io::Result<Vec<u8>> {
    // HEAD, gitfile and commondir are small metadata, never repository contents.
    let mut bytes = Vec::new();
    File::open(path)?.take(4097).read_to_end(&mut bytes)?;
    if bytes.len() > 4096 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "oversized Git context file",
        ));
    }
    if bytes.last() == Some(&b'\n') {
        bytes.pop();
    }
    if bytes.last() == Some(&b'\r') {
        bytes.pop();
    }
    Ok(bytes)
}
fn relative_path(base: &Path, bytes: &[u8]) -> Option<PathBuf> {
    if bytes.is_empty() || bytes.contains(&0) {
        return None;
    }
    let path = PathBuf::from(std::ffi::OsString::from_vec(bytes.to_vec()));
    fs::canonicalize(if path.is_absolute() {
        path
    } else {
        base.join(path)
    })
    .ok()
}
#[derive(Clone)]
struct Repository {
    root: PathBuf,
    git: PathBuf,
    common: PathBuf,
    linked: bool,
}
fn discover(cwd: &Path) -> Option<Repository> {
    // Nearest marker wins, including nested repositories and submodules.
    for root in cwd.ancestors().take(128) {
        let marker = root.join(".git");
        let metadata = match fs::metadata(&marker) {
            Ok(value) => value,
            Err(error) if error.kind() == io::ErrorKind::NotFound => continue,
            Err(_) => return None,
        };
        let git = if metadata.is_dir() {
            fs::canonicalize(&marker).ok()?
        } else {
            let bytes = read_small(&marker).ok()?;
            let path = bytes
                .strip_prefix(b"gitdir: ")
                .or_else(|| bytes.strip_prefix(b"gitdir:"))?;
            relative_path(root, path)?
        };
        let common = match read_small(&git.join("commondir")) {
            Ok(bytes) => relative_path(&git, &bytes)?,
            Err(error) if error.kind() == io::ErrorKind::NotFound => git.clone(),
            Err(_) => return None,
        };
        // A commondir marks linked worktrees; submodule gitfiles have no commondir.
        let linked = git != common;
        return Some(Repository {
            root: root.to_owned(),
            git,
            common,
            linked,
        });
    }
    None
}
fn head(repo: &Repository) -> Option<(String, bool)> {
    let path = repo.git.join("HEAD");
    // Git also accepts the historical HEAD symlink form.
    if let Ok(target) = fs::read_link(&path) {
        let bytes = target.as_os_str().as_bytes();
        if let Some(branch) = bytes.strip_prefix(b"refs/heads/") {
            return Some((bounded(&String::from_utf8_lossy(branch), 1024), false));
        }
    }
    let bytes = read_small(&path).ok()?;
    if let Some(branch) = bytes.strip_prefix(b"ref: refs/heads/") {
        let branch = std::str::from_utf8(branch).ok()?;
        if branch.is_empty() || branch.chars().any(char::is_control) {
            return None;
        }
        return Some((bounded(branch, 1024), false));
    }
    if matches!(bytes.len(), 40 | 64) && bytes.iter().all(u8::is_ascii_hexdigit) {
        return Some((String::from_utf8_lossy(&bytes[..8]).into_owned(), true));
    }
    None
}
fn basename(path: &Path) -> String {
    bounded(
        &path
            .file_name()
            .map(|s| s.to_string_lossy())
            .unwrap_or_default(),
        512,
    )
}
#[derive(Default)]
pub struct Cache {
    cwd: Option<PathBuf>,
    repo: Option<Repository>,
    checked: Option<Instant>,
}
impl Cache {
    pub fn sample(&mut self, cwd: Option<PathBuf>, remote: bool) -> Context {
        let Some(cwd) = cwd.filter(|_| !remote) else {
            self.cwd = None;
            self.repo = None;
            self.checked = None;
            return Context {
                remote,
                ..Context::default()
            };
        };
        let now = Instant::now();
        if self.cwd.as_ref() != Some(&cwd)
            || self
                .checked
                .is_none_or(|time| now.duration_since(time) >= Duration::from_secs(2))
        {
            self.repo = discover(&cwd);
            self.checked = Some(now);
            self.cwd = Some(cwd.clone());
        }
        let mut next = Context {
            cwd: bounded(&cwd.to_string_lossy(), 4096),
            ..Context::default()
        };
        if let Some(repo) = &self.repo {
            if let Some((branch, detached)) = head(repo) {
                next.repository =
                    if repo.linked && repo.common.file_name().is_some_and(|name| name == ".git") {
                        repo.common.parent().map(basename).unwrap_or_default()
                    } else {
                        basename(&repo.root)
                    };
                next.branch = branch;
                next.detached = detached;
                if repo.linked {
                    next.worktree = basename(&repo.root);
                }
            } else {
                self.checked = None;
            }
        }
        next
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn context_files_follow_worktree_head_without_git_subprocesses() {
        let root = std::env::temp_dir().join(format!("wb-context-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let repo = root.join("repository with spaces");
        let git = repo.join(".git");
        fs::create_dir_all(&git).unwrap();
        fs::write(git.join("HEAD"), "ref: refs/heads/main\n").unwrap();
        let mut cache = Cache::default();
        let initial = cache.sample(Some(repo.clone()), false);
        assert_eq!(initial.repository, "repository with spaces");
        assert_eq!(initial.branch, "main");
        fs::write(git.join("HEAD"), "ref: refs/heads/topic/λ\n").unwrap();
        assert_eq!(cache.sample(Some(repo.clone()), false).branch, "topic/λ");
        let tree = root.join("other checkout");
        let private = git.join("worktrees/other");
        fs::create_dir_all(&tree).unwrap();
        fs::create_dir_all(&private).unwrap();
        fs::write(
            tree.join(".git"),
            format!("gitdir: {}\n", private.display()),
        )
        .unwrap();
        fs::write(private.join("commondir"), "../..\n").unwrap();
        fs::write(
            private.join("HEAD"),
            "0123456789012345678901234567890123456789\n",
        )
        .unwrap();
        let linked = cache.sample(Some(tree), false);
        assert_eq!(linked.repository, initial.repository);
        assert_eq!(linked.worktree, "other checkout");
        assert_eq!(linked.branch, "01234567");
        assert!(linked.detached);
        assert_eq!(cache.sample(None, false), Context::default());
        assert!(cache.sample(Some(repo), true).remote);
        fs::remove_dir_all(root).unwrap();
    }
    #[test]
    fn payload_and_json_are_bounded_and_escape_metadata() {
        assert_eq!(bounded("éé", 3), "é");
        let context = Context {
            cwd: "/a\"b\\c\nd".into(),
            ..Context::default()
        };
        assert!(context.json().contains("/a\\\"b\\\\c\\u000ad"));
        let packet = context.payload();
        assert_eq!(&packet[..2], &[1, 1]);
        assert_eq!(
            u16::from_be_bytes([packet[2], packet[3]]) as usize,
            context.cwd.len()
        );
    }
}
