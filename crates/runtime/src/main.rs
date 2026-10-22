#[cfg(unix)]
fn main() {
    let mut args = std::env::args_os().skip(1);
    let Some(socket) = args.next() else {
        eprintln!("usage: workbench-session SOCKET [--login-shell] [SHELL]");
        std::process::exit(2);
    };
    if socket == "read" || socket == "snapshot" {
        let binary = socket == "snapshot";
        let Some(path) = args.next() else {
            eprintln!("usage: workbench-session read SOCKET | snapshot SOCKET FILE");
            std::process::exit(2);
        };
        let file = if binary { args.next() } else { None };
        if (binary && file.is_none()) || args.next().is_some() {
            eprintln!("usage: workbench-session read SOCKET | snapshot SOCKET FILE");
            std::process::exit(2);
        }
        let result = terminal_workbench_runtime::session::read_terminal(path.into(), binary)
            .and_then(|bytes| {
                if let Some(file) = file {
                    std::fs::write(file, bytes)?;
                } else {
                    std::io::Write::write_all(&mut std::io::stdout().lock(), &bytes)?;
                }
                Ok(())
            });
        if let Err(error) = result {
            eprintln!("terminal state: {error}");
            std::process::exit(1);
        }
        return;
    }
    if socket == "context" {
        let Some(path) = args.next() else {
            eprintln!("usage: workbench-session context SOCKET");
            std::process::exit(2);
        };
        if args.next().is_some() {
            std::process::exit(2);
        }
        match terminal_workbench_runtime::session::context(path.into()) {
            Ok(snapshot) => print!("{snapshot}"),
            Err(error) => {
                eprintln!("context: {error}");
                std::process::exit(1);
            }
        }
        return;
    }
    if socket == "inspect" {
        let Some(path) = args.next() else {
            eprintln!("usage: workbench-session inspect SOCKET");
            std::process::exit(2);
        };
        if args.next().is_some() {
            std::process::exit(2);
        }
        match terminal_workbench_runtime::session::inspect(path.into()) {
            Ok(snapshot) => print!("{snapshot}"),
            Err(error) => {
                eprintln!("inspect: {error}");
                std::process::exit(1);
            }
        }
        return;
    }
    if socket == "service" {
        if let Err(error) = service(args) {
            eprintln!("Session service: {error}");
            std::process::exit(1);
        }
        return;
    }
    if socket == "stop" {
        let Some(path) = args.next() else {
            eprintln!("usage: workbench-session stop SOCKET");
            std::process::exit(2);
        };
        if args.next().is_some() {
            std::process::exit(2);
        }
        if let Err(error) = terminal_workbench_runtime::session::stop(path.into()) {
            eprintln!("stop: {error}");
            std::process::exit(1);
        }
        return;
    }
    let first = args.next();
    let login = first
        .as_ref()
        .is_some_and(|argument| argument == "--login-shell");
    let explicit = if login { args.next() } else { first };
    let mut shell = explicit.map_or_else(
        terminal_workbench_runtime::shell::ShellLaunch::user_default,
        terminal_workbench_runtime::shell::ShellLaunch::explicit,
    );
    if login {
        shell = shell.with_login();
    }
    if args.next().is_some() {
        eprintln!("usage: workbench-session SOCKET [--login-shell] [SHELL]");
        std::process::exit(2);
    }
    if let Err(error) =
        terminal_workbench_runtime::session::serve_with_shell_launch(socket.into(), shell)
    {
        eprintln!("session: {error}");
        std::process::exit(1);
    }
}

#[cfg(unix)]
fn service(mut args: impl Iterator<Item = std::ffi::OsString>) -> Result<(), String> {
    const USAGE: &str =
        "usage: workbench-session service --state-dir DIR --runtime PATH --cwd DIR [--agent-bin DIR]";
    let (mut state, mut runtime, mut cwd, mut agent_bin) = (None, None, None, None);
    while let Some(flag) = args.next() {
        let slot = match flag.to_str() {
            Some("--state-dir") => &mut state,
            Some("--runtime") => &mut runtime,
            Some("--cwd") => &mut cwd,
            Some("--agent-bin") => &mut agent_bin,
            _ => return Err(USAGE.into()),
        };
        *slot = Some(std::path::PathBuf::from(args.next().ok_or(USAGE)?));
    }
    let (Some(state), Some(runtime), Some(cwd)) = (state, runtime, cwd) else {
        return Err(USAGE.into());
    };
    terminal_workbench_runtime::service::prepare_directory(&state)?;
    let canonical = |path: &std::path::Path| {
        std::fs::canonicalize(path).map_err(|error| format!("{}: {error}", path.display()))
    };
    terminal_workbench_runtime::service::serve(
        canonical(&state)?,
        canonical(&runtime)?,
        canonical(&cwd)?,
        agent_bin,
    )
}

#[cfg(not(unix))]
fn main() {
    eprintln!("the terminal integration spike currently targets macOS and Linux");
    std::process::exit(1);
}
