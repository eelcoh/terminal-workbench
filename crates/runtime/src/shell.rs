//! Shell startup policy, separate from the PTY transport and client environment.
use portable_pty::CommandBuilder;
use std::ffi::OsString;
use std::path::Path;

pub struct ShellLaunch {
    executable: OsString,
    login: bool,
    user_default: bool,
}

fn select_shell(
    environment: Option<OsString>,
    account: Option<OsString>,
    exists: impl Fn(&Path) -> bool,
) -> OsString {
    environment
        .into_iter()
        .chain(account)
        .find(|shell| exists(Path::new(shell)))
        .unwrap_or_else(|| "/bin/sh".into())
}

/// Some environments (Distrobox among them) set SHELL to a bare name; find it on PATH.
fn resolve_name(shell: OsString, path: Option<&std::ffi::OsStr>) -> OsString {
    if Path::new(&shell).components().count() != 1 || Path::new(&shell).is_absolute() {
        return shell;
    }
    path.into_iter()
        .flat_map(std::env::split_paths)
        .map(|directory| directory.join(&shell))
        .find(|candidate| candidate.is_absolute() && candidate.is_file())
        .map_or(shell, OsString::from)
}

fn supports_login(executable: &Path) -> bool {
    executable.file_name().is_some_and(|name| {
        matches!(
            name.to_str(),
            Some("zsh" | "bash" | "sh" | "fish" | "ksh" | "dash" | "csh" | "tcsh" | "nu")
        )
    })
}

impl ShellLaunch {
    pub fn explicit(executable: OsString) -> Self {
        Self {
            executable,
            login: false,
            user_default: false,
        }
    }

    pub fn user_default() -> Self {
        let environment = std::env::var_os("SHELL")
            .map(|shell| resolve_name(shell, std::env::var_os("PATH").as_deref()));
        let account = if environment
            .as_ref()
            .is_some_and(|path| Path::new(path).is_file())
        {
            None
        } else {
            crate::platform::account_shell()
        };
        let executable = select_shell(environment, account, Path::is_file);
        let login = crate::platform::DEFAULT_LOGIN_SHELL && supports_login(Path::new(&executable));
        Self {
            executable,
            login,
            user_default: true,
        }
    }

    /// Explicit CLI opt-in, also used to exercise the Mac policy with real shells
    /// on Linux without changing that platform's default startup behavior.
    pub fn with_login(mut self) -> Self {
        self.login = true;
        self
    }

    pub(crate) fn command(self) -> CommandBuilder {
        let mut command = CommandBuilder::new(&self.executable);
        if self.user_default {
            command.env("SHELL", &self.executable);
        }
        if self.login {
            command.arg("-l");
        }
        command
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn configured_shell_precedes_account_and_invalid_environment_falls_back() {
        let valid = |path: &Path| path == Path::new("/bin/zsh") || path == Path::new("/bin/bash");
        assert_eq!(
            select_shell(Some("/bin/bash".into()), Some("/bin/zsh".into()), valid),
            "/bin/bash"
        );
        assert_eq!(
            select_shell(None, Some("/bin/zsh".into()), valid),
            "/bin/zsh"
        );
        assert_eq!(
            select_shell(Some("/missing".into()), Some("/bin/zsh".into()), valid),
            "/bin/zsh"
        );
        assert_eq!(
            select_shell(None, Some("/missing".into()), valid),
            "/bin/sh"
        );
    }

    #[test]
    fn bare_shell_name_is_found_on_path() {
        let directory = std::env::temp_dir().join(format!("band-shell-{}", std::process::id()));
        std::fs::create_dir_all(&directory).unwrap();
        std::fs::write(directory.join("zsh"), "").unwrap();
        let path = std::env::join_paths(["relative", directory.to_str().unwrap()]).unwrap();
        assert_eq!(
            resolve_name("zsh".into(), Some(&path)),
            directory.join("zsh")
        );
        assert_eq!(resolve_name("fish".into(), Some(&path)), "fish");
        assert_eq!(resolve_name("/bin/zsh".into(), Some(&path)), "/bin/zsh");
        assert_eq!(resolve_name("zsh".into(), None), "zsh");
        std::fs::remove_dir_all(directory).unwrap();
    }

    #[test]
    fn automatic_login_is_only_for_known_shells() {
        assert!(supports_login(Path::new("/bin/zsh")));
        assert!(supports_login(Path::new("/opt/homebrew/bin/fish")));
        assert!(!supports_login(Path::new("/tools/custom-reader")));
        assert!(!supports_login(Path::new("/usr/bin/python3")));
    }
}
