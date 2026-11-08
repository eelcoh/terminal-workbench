//! Cached local process/Git context. File discovery runs off the PTY event thread.
use std::fs::{self, File, OpenOptions};
use std::io::{self, Read, Write};
use std::os::fd::AsRawFd;
use std::os::unix::ffi::{OsStrExt, OsStringExt};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

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

/// How long to wait before the next scheduled `git status` after one that took
/// `cost`: twenty times as long, from five seconds up to two minutes.
fn status_interval(cost: Duration) -> Duration {
    (cost * 20).clamp(Duration::from_secs(5), Duration::from_secs(120))
}
fn unix_nanos(time: SystemTime) -> u128 {
    time.duration_since(UNIX_EPOCH)
        .map(|value| value.as_nanos())
        .unwrap_or(0)
}
/// FNV-1a: a stable file name for a worktree root, the same in every build.
fn root_key(root: &Path) -> String {
    let mut hash: u64 = 0xcbf2_9ce4_8422_2325;
    for byte in root.as_os_str().as_bytes() {
        hash = (hash ^ u64::from(*byte)).wrapping_mul(0x0100_0000_01b3);
    }
    format!("{hash:016x}")
}

/// One worktree's last `git status`, shared by the runtimes of a state folder
/// so panes in the same worktree run it once. A file per worktree under
/// `<state>/git-status/`: `1 <dirty 0|1|-> <started, Unix ns> <cost, ms>`,
/// then the root on the next line to rule out a hash collision.
#[derive(Debug, PartialEq)]
struct Shared {
    dirty: Option<bool>,
    started: u128,
    cost: Duration,
}
impl Shared {
    fn encode(&self, root: &Path) -> Vec<u8> {
        let dirty = match self.dirty {
            Some(true) => "1",
            Some(false) => "0",
            None => "-",
        };
        let mut bytes =
            format!("1 {dirty} {} {}\n", self.started, self.cost.as_millis()).into_bytes();
        bytes.extend_from_slice(root.as_os_str().as_bytes());
        bytes
    }
    fn decode(bytes: &[u8], root: &Path) -> Option<Shared> {
        let newline = bytes.iter().position(|byte| *byte == b'\n')?;
        if &bytes[newline + 1..] != root.as_os_str().as_bytes() {
            return None;
        }
        let line = std::str::from_utf8(&bytes[..newline]).ok()?;
        let mut fields = line.split(' ');
        if fields.next()? != "1" {
            return None;
        }
        let dirty = match fields.next()? {
            "1" => Some(true),
            "0" => Some(false),
            "-" => None,
            _ => return None,
        };
        let started = fields.next()?.parse().ok()?;
        let cost = Duration::from_millis(fields.next()?.parse().ok()?);
        fields.next().is_none().then_some(Shared {
            dirty,
            started,
            cost,
        })
    }
}
/// What one status round decided: a result, or nothing yet because another
/// runtime is running `git status` for this worktree right now.
enum Round {
    Result(Shared),
    Busy,
}
fn private_dir(path: &Path) -> Option<()> {
    match fs::DirBuilder::new().mode(0o700).create(path) {
        Ok(()) => {}
        Err(error) if error.kind() == io::ErrorKind::AlreadyExists => {}
        Err(_) => return None,
    }
    fs::symlink_metadata(path).ok()?.is_dir().then_some(())
}
fn open_private(path: &Path, write: bool) -> io::Result<File> {
    let mut options = OpenOptions::new();
    options.read(true).custom_flags(libc::O_NOFOLLOW);
    if write {
        options.write(true).create(true).mode(0o600);
    }
    options.open(path)
}

#[derive(Default)]
pub struct Cache {
    cwd: Option<PathBuf>,
    repo: Option<Repository>,
    checked: Option<Instant>,
    // `git status` is rate-limited per worktree (see `status_interval`), and
    // checked early when the index changes or the foreground job changes, as
    // long as the last check cost less than a quarter of the time since.
    status_root: Option<PathBuf>,
    status_next: Option<Instant>,
    status_ran: Option<Instant>,
    status_cost: Duration,
    index_seen: Option<SystemTime>,
    // An early check is due; a result must have started after this (Unix ns).
    pending: Option<u128>,
    dirty: Option<bool>,
    shared: Option<PathBuf>,
    #[cfg(test)]
    runs: u32,
}
impl Cache {
    /// A cache sharing `git status` results through `directory` with the other
    /// runtimes of the same state folder.
    pub fn shared(directory: PathBuf) -> Cache {
        Cache {
            shared: Some(directory),
            ..Cache::default()
        }
    }
    /// `job_changed`: the terminal's foreground job started or ended since the
    /// last sample, so a command may have changed the worktree.
    pub fn sample(&mut self, cwd: Option<PathBuf>, remote: bool, job_changed: bool) -> Context {
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
        if let Some(repo) = self.repo.clone() {
            if let Some((branch, detached)) = head(&repo) {
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
                self.status(&repo, now, job_changed);
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
    fn status(&mut self, repo: &Repository, now: Instant, job_changed: bool) {
        // Commits, adds, checkouts and stashes all rewrite the index.
        let index = fs::metadata(repo.git.join("index"))
            .and_then(|metadata| metadata.modified())
            .ok();
        let moved = self.status_root.as_ref() != Some(&repo.root);
        if moved {
            self.status_root = Some(repo.root.clone());
            self.status_next = None;
            self.status_ran = None;
            self.pending = None;
            self.dirty = None;
        } else if job_changed {
            // The job may have ended up to a second before this sample saw it.
            let since =
                unix_nanos(SystemTime::now()).saturating_sub(Duration::from_secs(1).as_nanos());
            self.pending = Some(self.pending.unwrap_or(0).max(since));
        } else if index != self.index_seen {
            self.pending = Some(self.pending.unwrap_or(0));
        }
        self.index_seen = index;
        let early = self.pending.is_some()
            && self.status_ran.is_none_or(|time| {
                now.duration_since(time) >= (self.status_cost * 4).max(Duration::from_secs(1))
            });
        if !(moved || early || self.status_next.is_none_or(|time| now >= time)) {
            return;
        }
        // A result started after the index last changed and after a job change,
        // and within its own interval, is as good as running Git again.
        let since = index
            .map(unix_nanos)
            .unwrap_or(0)
            .max(self.pending.unwrap_or(0));
        let fresh = |result: &Shared| {
            let age = unix_nanos(SystemTime::now()).saturating_sub(result.started);
            result.started >= since && age < status_interval(result.cost).as_nanos()
        };
        match self.round(&repo.root, fresh) {
            Round::Busy => {}
            Round::Result(result) => {
                let age = Duration::from_nanos(
                    unix_nanos(SystemTime::now()).saturating_sub(result.started) as u64,
                );
                self.dirty = result.dirty;
                self.status_cost = result.cost;
                self.status_ran = Some(now.checked_sub(age).unwrap_or(now));
                self.status_next = Some(now + status_interval(result.cost).saturating_sub(age));
                self.pending = None;
            }
        }
    }
    /// A fresh shared result, or `git status` run here and shared.
    fn round(&mut self, root: &Path, fresh: impl Fn(&Shared) -> bool) -> Round {
        let Some(directory) = self
            .shared
            .clone()
            .filter(|path| private_dir(path).is_some())
        else {
            return Round::Result(self.run(root));
        };
        let path = directory.join(root_key(root));
        let read = |path: &Path| {
            let mut bytes = Vec::new();
            open_private(path, false)
                .and_then(|file| file.take(8192).read_to_end(&mut bytes))
                .ok()
                .and_then(|_| Shared::decode(&bytes, root))
        };
        if let Some(result) = read(&path).filter(&fresh) {
            return Round::Result(result);
        }
        let Ok(lock) = open_private(&path.with_extension("lock"), true) else {
            return Round::Result(self.run(root));
        };
        // SAFETY: the descriptor is open for the lifetime of `lock`.
        if unsafe { libc::flock(lock.as_raw_fd(), libc::LOCK_EX | libc::LOCK_NB) } != 0 {
            return if io::Error::last_os_error().raw_os_error() == Some(libc::EWOULDBLOCK) {
                Round::Busy
            } else {
                Round::Result(self.run(root))
            };
        }
        // Another runtime may have finished while this one waited for the lock.
        if let Some(result) = read(&path).filter(&fresh) {
            return Round::Result(result);
        }
        let result = self.run(root);
        let temporary = path.with_extension(format!("{}.tmp", std::process::id()));
        let written = open_private(&temporary, true).and_then(|mut file| {
            file.set_len(0)?;
            file.write_all(&result.encode(root))
        });
        if written
            .and_then(|()| fs::rename(&temporary, &path))
            .is_err()
        {
            let _ = fs::remove_file(&temporary);
        }
        drop(lock);
        Round::Result(result)
    }
    fn run(&mut self, root: &Path) -> Shared {
        #[cfg(test)]
        {
            self.runs += 1;
        }
        let started = SystemTime::now();
        let clock = Instant::now();
        let dirty = uncommitted(root, Duration::from_secs(2));
        Shared {
            dirty,
            started: unix_nanos(started),
            cost: clock.elapsed(),
        }
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
        let initial = cache.sample(Some(repo.clone()), false, false);
        assert_eq!(initial.repository, "repository with spaces");
        assert_eq!(initial.branch, "main");
        fs::write(git.join("HEAD"), "ref: refs/heads/topic/λ\n").unwrap();
        assert_eq!(
            cache.sample(Some(repo.clone()), false, false).branch,
            "topic/λ"
        );
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
        let linked = cache.sample(Some(tree), false, false);
        assert_eq!(linked.repository, initial.repository);
        assert_eq!(linked.worktree, "other checkout");
        assert_eq!(linked.branch, "01234567");
        assert!(linked.detached);
        assert_eq!(cache.sample(None, false, false), Context::default());
        assert!(cache.sample(Some(repo), true, false).remote);
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
        assert_eq!(
            cache.sample(Some(root.clone()), false, false).dirty,
            Some(true)
        );
        assert_eq!(
            uncommitted(&root.join("missing"), Duration::from_secs(5)),
            None
        );
        fs::remove_dir_all(root).unwrap();
    }
    #[test]
    fn panes_in_one_worktree_share_git_status_and_the_index_triggers_it() {
        if Command::new("git").arg("--version").output().is_err() {
            return;
        }
        let base = std::env::temp_dir().join(format!("wb-shared-{}", std::process::id()));
        let _ = fs::remove_dir_all(&base);
        let root = base.join("repo");
        fs::create_dir_all(&root).unwrap();
        let root = fs::canonicalize(root).unwrap();
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
        fs::write(root.join("notes.txt"), "draft").unwrap();
        git(&["add", "notes.txt"]);
        git(&[
            "-c",
            "user.name=Test",
            "-c",
            "user.email=test@example.invalid",
            "commit",
            "-q",
            "-m",
            "fixture",
        ]);
        let shared = base.join("git-status");
        let mut first = Cache::shared(shared.clone());
        let mut second = Cache::shared(shared.clone());
        assert_eq!(
            first.sample(Some(root.clone()), false, false).dirty,
            Some(false)
        );
        assert_eq!(
            second.sample(Some(root.clone()), false, false).dirty,
            Some(false)
        );
        assert_eq!(
            (first.runs, second.runs),
            (1, 0),
            "the second pane reuses the result"
        );

        // Staging rewrites the index. Within a second of the last check both
        // panes wait; then one checks early and the other reuses its result.
        fs::write(root.join("notes.txt"), "changed").unwrap();
        git(&["add", "notes.txt"]);
        assert_eq!(
            second.sample(Some(root.clone()), false, false).dirty,
            Some(false)
        );
        std::thread::sleep(Duration::from_millis(1100));
        assert_eq!(
            second.sample(Some(root.clone()), false, false).dirty,
            Some(true)
        );
        assert_eq!(
            first.sample(Some(root.clone()), false, false).dirty,
            Some(true)
        );
        assert_eq!((first.runs, second.runs), (1, 1));

        // A job change asks for a result newer than itself.
        std::thread::sleep(Duration::from_millis(1100));
        assert_eq!(
            first.sample(Some(root.clone()), false, true).dirty,
            Some(true)
        );
        assert_eq!(first.runs, 2);

        // An unreadable or foreign entry is ignored, not trusted.
        let entry = shared.join(root_key(&root));
        assert!(Shared::decode(&fs::read(&entry).unwrap(), &root).is_some());
        assert!(Shared::decode(&fs::read(&entry).unwrap(), &root.join("other")).is_none());
        assert!(Shared::decode(b"2 1 0 0\n/x", Path::new("/x")).is_none());
        fs::remove_dir_all(base).unwrap();
    }
}
