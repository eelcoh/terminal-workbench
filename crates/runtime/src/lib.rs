//! Runtime-owned sessions, independent of desktop windows.
//!
//! The current Unix-only integration spike hosts one PTY and replays its output
//! and resize history to a reconnecting client. It is not a production protocol.

#[cfg(unix)]
mod agent;
#[cfg(unix)]
mod attention;
#[cfg(unix)]
mod context;
#[cfg(unix)]
mod input_queue;
#[cfg(unix)]
mod lifecycle;
#[cfg(unix)]
mod platform;
#[cfg(unix)]
pub mod session;
#[cfg(unix)]
mod terminal;
