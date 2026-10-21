//! Conservative foreground process identification, independent of screen/output.
use crate::platform::ProcessIdentity;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Agent {
    None,
    Codex,
    Claude,
    Gemini,
    OpenCode,
    Aider,
    Copilot,
    Pi,
    Unknown,
}

impl Agent {
    pub fn tag(self) -> u8 {
        match self {
            Self::None => 0,
            Self::Codex => 1,
            Self::Claude => 2,
            Self::Gemini => 3,
            Self::OpenCode => 4,
            Self::Aider => 5,
            Self::Copilot => 6,
            Self::Pi => 7,
            Self::Unknown => 255,
        }
    }
    pub fn json(self) -> &'static str {
        match self {
            Self::None => "{\"agent\":null,\"available\":true,\"source\":\"foreground-process\"}\n",
            Self::Unknown => {
                "{\"agent\":null,\"available\":false,\"source\":\"foreground-process\"}\n"
            }
            Self::Codex => {
                "{\"agent\":\"codex\",\"available\":true,\"source\":\"foreground-process\"}\n"
            }
            Self::Claude => {
                "{\"agent\":\"claude\",\"available\":true,\"source\":\"foreground-process\"}\n"
            }
            Self::Gemini => {
                "{\"agent\":\"gemini\",\"available\":true,\"source\":\"foreground-process\"}\n"
            }
            Self::OpenCode => {
                "{\"agent\":\"opencode\",\"available\":true,\"source\":\"foreground-process\"}\n"
            }
            Self::Copilot => {
                "{\"agent\":\"copilot\",\"available\":true,\"source\":\"foreground-process\"}\n"
            }
            Self::Pi => "{\"agent\":\"pi\",\"available\":true,\"source\":\"foreground-process\"}\n",
            Self::Aider => {
                "{\"agent\":\"aider\",\"available\":true,\"source\":\"foreground-process\"}\n"
            }
        }
    }
}

pub fn identify(process: Option<ProcessIdentity>) -> Agent {
    let Some(process) = process else {
        return Agent::Unknown;
    };
    let name = process
        .executable
        .file_name()
        .and_then(|name| name.to_str())
        .unwrap_or("");
    // The native Claude installer uses a version-named executable; the public
    // claude command is a symlink. Recognize only its specific install directory
    // and a numeric three-component version, not arbitrary version-named files.
    if process
        .executable
        .parent()
        .is_some_and(|path| path.ends_with(".local/share/claude/versions"))
        && name.split('.').count() == 3
        && name
            .split('.')
            .all(|part| !part.is_empty() && part.bytes().all(|byte| byte.is_ascii_digit()))
    {
        return Agent::Claude;
    }
    let direct = match name {
        "codex" => Agent::Codex,
        "claude" => Agent::Claude,
        "gemini" => Agent::Gemini,
        "opencode" => Agent::OpenCode,
        "aider" => Agent::Aider,
        "copilot" => Agent::Copilot,
        "pi" => Agent::Pi,
        _ => Agent::None,
    };
    if direct != Agent::None {
        return direct;
    }
    // Pi rewrites Node argv to a single exact public process title. Retain
    // the executable boundary: an unrelated native program named in argv is not Pi.
    if (name == "node" || name == "nodejs")
        && process.script.is_none()
        && process.invocation.as_deref() == Some(std::path::Path::new("pi"))
    {
        return Agent::Pi;
    }
    let Some(script) = process.script else {
        return Agent::None;
    };
    if name == "node" || name == "nodejs" {
        if script.ends_with("@earendil-works/pi-coding-agent/dist/bundle/cli.js")
            || script.ends_with("@earendil-works/pi-coding-agent/dist/cli.js")
            || script.ends_with("@mariozechner/pi-coding-agent/dist/cli.js")
        {
            return Agent::Pi;
        }
        if script.ends_with("@github/copilot/npm-loader.js") {
            return Agent::Copilot;
        }
        if script.ends_with("@openai/codex/bin/codex.js") {
            return Agent::Codex;
        }
        if script.ends_with("@anthropic-ai/claude-code/cli.js") {
            return Agent::Claude;
        }
        if script.ends_with("@google/gemini-cli/dist/index.js")
            || script.ends_with("@google/gemini-cli/bundle/gemini.js")
        {
            return Agent::Gemini;
        }
    }
    if (name == "python" || name == "python3" || name.starts_with("python3."))
        && script.file_name().is_some_and(|name| name == "aider")
    {
        return Agent::Aider;
    }
    Agent::None
}

#[cfg(test)]
mod tests {
    use super::*;
    fn process(executable: &str, script: Option<&str>) -> Option<ProcessIdentity> {
        Some(ProcessIdentity {
            executable: executable.into(),
            script: script.map(Into::into),
            invocation: None,
        })
    }
    #[test]
    fn identification_uses_executable_and_launcher_boundaries() {
        assert_eq!(identify(None), Agent::Unknown);
        assert_eq!(
            identify(Some(ProcessIdentity {
                executable: "/bin/node".into(),
                script: None,
                invocation: Some("pi".into())
            })),
            Agent::Pi
        );
        assert_eq!(
            identify(Some(ProcessIdentity {
                executable: "/bin/echo".into(),
                script: None,
                invocation: Some("pi".into())
            })),
            Agent::None
        );
        assert_eq!(
            identify(Some(ProcessIdentity {
                executable: "/bin/node".into(),
                script: None,
                invocation: Some("pi unrelated".into())
            })),
            Agent::None
        );

        assert_eq!(identify(process("/opt/bin/pi", None)), Agent::Pi);
        assert_eq!(
            identify(process(
                "/bin/node",
                Some("/pkg/@earendil-works/pi-coding-agent/dist/bundle/cli.js")
            )),
            Agent::Pi
        );
        assert_eq!(
            identify(process(
                "/bin/node",
                Some("/pkg/pi-coding-agent/dist/bundle/cli.js")
            )),
            Agent::None
        );
        assert_eq!(
            identify(process(
                "/bin/echo",
                Some("/pkg/@earendil-works/pi-coding-agent/dist/cli.js")
            )),
            Agent::None
        );

        assert_eq!(Agent::Codex.tag(), 1);
        assert_eq!(Agent::Claude.tag(), 2);
        assert_eq!(Agent::Aider.tag(), 5);
        assert_eq!(Agent::Copilot.tag(), 6);
        assert_eq!(identify(process("/opt/bin/copilot", None)), Agent::Copilot);
        assert_eq!(identify(process("/opt/bin/not-copilot", None)), Agent::None);
        assert_eq!(
            identify(process(
                "/usr/bin/node",
                Some("/pkg/@github/copilot/npm-loader.js")
            )),
            Agent::Copilot
        );
        assert_eq!(
            identify(process("/usr/bin/node", Some("/pkg/copilot/npm-loader.js"))),
            Agent::None
        );
        assert_eq!(
            identify(process(
                "/bin/sh",
                Some("/pkg/@github/copilot/npm-loader.js")
            )),
            Agent::None
        );

        assert_eq!(
            identify(process(
                "/home/user/.local/share/claude/versions/2.1.289",
                None
            )),
            Agent::Claude
        );
        assert_eq!(
            identify(process("/other/versions/2.1.289", None)),
            Agent::None
        );
        assert_eq!(
            identify(process(
                "/home/user/.local/share/claude/versions/not-a-version",
                None
            )),
            Agent::None
        );
        assert_eq!(identify(process("/bin/sh", None)), Agent::None);
        assert_eq!(identify(process("/bin/codex", None)), Agent::Codex);
        assert_eq!(identify(process("/bin/not-codex", None)), Agent::None);
        assert_eq!(
            identify(process(
                "/bin/node",
                Some("/pkg/@openai/codex/bin/codex.js")
            )),
            Agent::Codex
        );
        assert_eq!(
            identify(process("/bin/node", Some("/my/codex.js"))),
            Agent::None
        );
        assert_eq!(
            identify(process(
                "/bin/echo",
                Some("/pkg/@openai/codex/bin/codex.js")
            )),
            Agent::None
        );
        assert_eq!(
            identify(process("/bin/python3.14", Some("/bin/aider"))),
            Agent::Aider
        );
        assert_eq!(
            identify(process("/bin/python-unrelated", Some("/bin/aider"))),
            Agent::None
        );
    }
}
