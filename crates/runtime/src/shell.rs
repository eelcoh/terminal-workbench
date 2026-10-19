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
        let environment = std::env::var_os("SHELL");
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
    fn automatic_login_is_only_for_known_shells() {
        assert!(supports_login(Path::new("/bin/zsh")));
        assert!(supports_login(Path::new("/opt/homebrew/bin/fish")));
        assert!(!supports_login(Path::new("/tools/custom-reader")));
        assert!(!supports_login(Path::new("/usr/bin/python3")));
    }
}
