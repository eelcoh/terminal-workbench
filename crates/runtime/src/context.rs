//! Cached local process/Git context. File discovery runs off the PTY event thread.
use std::fs::{self, File};
use std::io::{self, Read, Write};
use std::os::unix::ffi::{OsStrExt, OsStringExt};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Context {
    pub cwd: String,
    pub repository: String,
    pub branch: String,
    pub worktree: String,
    pub detached: bool,
    pub remote: bool,
    /// Uncommitted changes in the worktree, from `git status`; None when unknown.
    pub dirty: Option<bool>,
}

fn bounded(text: &str, maximum: usize) -> String {
    let mut end = text.len().min(maximum);
    while !text.is_char_boundary(end) {
        end -= 1;
    }
    text[..end].to_owned()
}
pub(crate) fn json_string(text: &str) -> String {
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
        format!("{{\"available\":{},\"source\":\"{}\",\"cwd\":{},\"repository\":{},\"branch\":{},\"worktree\":{},\"detached\":{},\"dirty\":{}}}\n",
            !self.cwd.is_empty(),if self.remote {"remote-unavailable"} else {"local-process"},
            json_string(&self.cwd),json_string(&self.repository),json_string(&self.branch),json_string(&self.worktree),self.detached,
            self.dirty.map_or("null".to_owned(),|dirty| dirty.to_string()))
    }
    /// CTX1 keeps its three flags; CTX2 adds bit 3 (dirty state known) and
    /// bit 4 (uncommitted changes).
    pub fn payload(&self, version: u8) -> Vec<u8> {
        let mut flags = u8::from(!self.cwd.is_empty())
            | (u8::from(self.detached) << 1)
            | (u8::from(self.remote) << 2);
        if version >= 2 {
            if let Some(dirty) = self.dirty {
                flags |= 8 | (u8::from(dirty) << 4);
            }
        }
        let mut result = vec![1, flags];
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
/// Whether the worktree at `root` has uncommitted changes, including untracked
/// files. Runs `git status` with a time limit; None when Git is missing, refuses
/// the repository or is too slow. The fsmonitor hook is turned off so a
/// repository's own configuration cannot make the check start other programs.
fn uncommitted(root: &Path, limit: Duration) -> Option<bool> {
    let mut child = Command::new("git")
        .arg("-c")
        .arg("core.fsmonitor=false")
        .arg("--no-optional-locks")
        .arg("-C")
        .arg(root)
        .args(["status", "--porcelain", "-z", "--ignore-submodules=dirty"])
        .env("GIT_OPTIONAL_LOCKS", "0")
        .env("GIT_TERMINAL_PROMPT", "0")
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::null())
        .spawn()
        .ok()?;
    // Drain output on its own thread so a large status cannot block Git.
    let mut stdout = child.stdout.take()?;
    let reader = std::thread::spawn(move || {
        struct Count(u64);
        impl Write for Count {
            fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
                self.0 += bytes.len() as u64;
                Ok(bytes.len())
            }
            fn flush(&mut self) -> io::Result<()> {
                Ok(())
            }
        }
        let mut count = Count(0);
        io::copy(&mut stdout, &mut count)
            .map(|_| count.0)
            .unwrap_or(0)
    });
    let deadline = Instant::now() + limit;
    let status = loop {
        match child.try_wait() {
            Ok(Some(status)) => break Some(status),
            Ok(None) if Instant::now() < deadline => std::thread::sleep(Duration::from_millis(20)),
            _ => {
                let _ = child.kill();
                let _ = child.wait();
                break None;
            }
        }
    };
    let bytes = reader.join().unwrap_or(0);
    status.filter(|status| status.success()).map(|_| bytes > 0)
}

#[derive(Default)]
pub struct Cache {
    cwd: Option<PathBuf>,
    repo: Option<Repository>,
    checked: Option<Instant>,
    // `git status` is rate-limited per worktree: at most every five seconds,
    // and slower when it takes long (twenty times its duration, up to two minutes).
    status_root: Option<PathBuf>,
    status_next: Option<Instant>,
    dirty: Option<bool>,
}
impl Cache {
    pub fn sample(&mut self, cwd: Option<PathBuf>, remote: bool) -> Context {
        let Some(cwd) = cwd.filter(|_| !remote) else {
            self.cwd = None;
            self.repo = None;
            self.checked = None;
            self.status_root = None;
            self.dirty = None;
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
                if self.status_root.as_ref() != Some(&repo.root)
                    || self.status_next.is_none_or(|time| now >= time)
                {
                    let started = Instant::now();
                    self.dirty = uncommitted(&repo.root, Duration::from_secs(2));
                    let interval = (started.elapsed() * 20)
                        .clamp(Duration::from_secs(5), Duration::from_secs(120));
                    self.status_root = Some(repo.root.clone());
                    self.status_next = Some(Instant::now() + interval);
                }
                next.dirty = self.dirty;
            } else {
                self.checked = None;
            }
        } else {
            self.status_root = None;
            self.dirty = None;
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
        let packet = context.payload(1);
        assert_eq!(&packet[..2], &[1, 1]);
        let dirty = Context {
            dirty: Some(true),
            ..context.clone()
        };
        assert_eq!(&dirty.payload(1)[..2], &[1, 1]);
        assert_eq!(&dirty.payload(2)[..2], &[1, 1 | 8 | 16]);
        assert_eq!(
            &Context {
                dirty: Some(false),
                ..context.clone()
            }
            .payload(2)[..2],
            &[1, 1 | 8]
        );
        assert!(
            dirty.json().contains("\"dirty\":true") && context.json().contains("\"dirty\":null")
        );
        assert_eq!(
            u16::from_be_bytes([packet[2], packet[3]]) as usize,
            context.cwd.len()
        );
    }
    #[test]
    fn uncommitted_changes_come_from_git_status() {
        if Command::new("git").arg("--version").output().is_err() {
            return;
        }
        let root = std::env::temp_dir().join(format!("wb-dirty-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let git = |args: &[&str]| {
            assert!(Command::new("git")
                .arg("-C")
                .arg(&root)
                .args(args)
                .output()
                .unwrap()
                .status
                .success())
        };
        git(&["init", "-q"]);
        git(&[
            "-c",
            "user.name=Test",
            "-c",
            "user.email=test@example.invalid",
            "commit",
            "-q",
            "--allow-empty",
            "-m",
            "fixture",
        ]);
        assert_eq!(uncommitted(&root, Duration::from_secs(5)), Some(false));
        fs::write(root.join("notes.txt"), "draft").unwrap();
        assert_eq!(uncommitted(&root, Duration::from_secs(5)), Some(true));
        let mut cache = Cache::default();
        assert_eq!(cache.sample(Some(root.clone()), false).dirty, Some(true));
        assert_eq!(
            uncommitted(&root.join("missing"), Duration::from_secs(5)),
            None
        );
        fs::remove_dir_all(root).unwrap();
    }
}
