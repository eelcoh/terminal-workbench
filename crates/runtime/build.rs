use std::{env, path::PathBuf, process::Command};

fn run(command: &mut Command) {
    let status = command
        .status()
        .expect("could not start the native compiler");
    assert!(status.success(), "native terminal adapter build failed");
}
fn main() {
    println!("cargo:rerun-if-changed=native/terminal_core.c");
    println!("cargo:rerun-if-env-changed=CC");
    println!("cargo:rerun-if-env-changed=AR");
    if env::var("CARGO_CFG_TARGET_FAMILY").as_deref() != Ok("unix") {
        return;
    }
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").expect("Cargo package directory"));
    let ghostty = root.join("../../.local/runtime-vt");
    let library = ghostty.join("lib/libghostty-vt.a");
    assert!(
        library.is_file(),
        "prepare pinned libghostty-vt first: run just runtime-build"
    );
    println!("cargo:rerun-if-changed={}", library.display());
    let out = PathBuf::from(env::var_os("OUT_DIR").expect("Cargo output directory"));
    run(
        Command::new(env::var_os("CC").unwrap_or_else(|| "cc".into()))
            .args([
                "-std=c11", "-O2", "-fPIC", "-Wall", "-Wextra", "-Werror", "-c",
            ])
            .arg(root.join("native/terminal_core.c"))
            .arg("-I")
            .arg(ghostty.join("include"))
            .arg("-o")
            .arg(out.join("terminal_core.o")),
    );
    run(
        Command::new(env::var_os("AR").unwrap_or_else(|| "ar".into()))
            .arg("crs")
            .arg(out.join("libworkbench-vt.a"))
            .arg(out.join("terminal_core.o")),
    );
    // The VT build installs a shared library beside the archive. Apple's linker
    // prefers it for -l, so link from a directory holding only the archive.
    std::fs::copy(&library, out.join("libghostty-vt.a")).expect("copy libghostty-vt.a");
    println!("cargo:rustc-link-search=native={}", out.display());
    println!("cargo:rustc-link-lib=static=workbench-vt");
    println!("cargo:rustc-link-lib=static=ghostty-vt");
}
