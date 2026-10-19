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

#[cfg(not(unix))]
fn main() {
    eprintln!("the terminal integration spike currently targets macOS and Linux");
    std::process::exit(1);
}
