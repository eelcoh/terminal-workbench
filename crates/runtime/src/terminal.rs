//! Runtime-owned terminal state. All FFI access stays on the session event thread.
use std::{ffi::c_void, io, ptr::NonNull};

const MAX_EXPORT: usize = 64 * 1024 * 1024;
unsafe extern "C" {
    fn wb_vt_new() -> *mut c_void;
    fn wb_vt_free(state: *mut c_void);
    fn wb_vt_feed(state: *mut c_void, bytes: *const u8, length: usize, replies: bool) -> i32;
    fn wb_vt_replies(state: *mut c_void, length: *mut usize) -> *const u8;
    fn wb_vt_resize(state: *mut c_void, cols: u16, rows: u16) -> i32;
    fn wb_vt_export(
        state: *mut c_void,
        binary: bool,
        bytes: *mut *mut u8,
        length: *mut usize,
    ) -> i32;
    fn wb_vt_buffer_free(bytes: *mut u8, length: usize);
    #[cfg(test)]
    fn wb_vt_restore(bytes: *const u8, length: usize) -> *mut c_void;
}
fn failure() -> io::Error {
    io::Error::other("runtime terminal operation failed")
}

pub(crate) struct Terminal {
    state: NonNull<c_void>,
}
impl Terminal {
    pub fn new() -> io::Result<Self> {
        // SAFETY: the adapter returns a uniquely owned opaque context or NULL.
        NonNull::new(unsafe { wb_vt_new() })
            .map(|state| Self { state })
            .ok_or_else(failure)
    }
    pub fn feed(&mut self, bytes: &[u8], replies: bool) -> io::Result<Vec<u8>> {
        // SAFETY: the context is live and uniquely borrowed; bytes lasts through this synchronous call.
        if unsafe { wb_vt_feed(self.state.as_ptr(), bytes.as_ptr(), bytes.len(), replies) } != 0 {
            return Err(failure());
        }
        let mut length = 0;
        // SAFETY: the adapter's fixed reply buffer lives until the next mutable operation.
        let pointer = unsafe { wb_vt_replies(self.state.as_ptr(), &mut length) };
        if length == 0 {
            return Ok(Vec::new());
        }
        // SAFETY: the adapter supplies exactly length initialized bytes, copied before any next call.
        Ok(unsafe { std::slice::from_raw_parts(pointer, length) }.to_vec())
    }
    pub fn resize(&mut self, cols: u16, rows: u16) -> io::Result<()> {
        // SAFETY: the context is uniquely borrowed and geometry is validated by the session codec.
        if unsafe { wb_vt_resize(self.state.as_ptr(), cols, rows) } == 0 {
            Ok(())
        } else {
            Err(failure())
        }
    }
    pub fn export(&mut self, binary: bool) -> io::Result<Vec<u8>> {
        let mut pointer = std::ptr::null_mut();
        let mut length = 0;
        // SAFETY: both output pointers are valid; allocation ownership is returned by the adapter.
        let result =
            unsafe { wb_vt_export(self.state.as_ptr(), binary, &mut pointer, &mut length) };
        let bytes = if result != 0 || length > MAX_EXPORT || (length > 0 && pointer.is_null()) {
            Err(failure())
        } else if length == 0 {
            Ok(Vec::new())
        } else {
            // SAFETY: a successful export owns length initialized bytes until the matching free below.
            Ok(unsafe { std::slice::from_raw_parts(pointer, length) }.to_vec())
        };
        // SAFETY: the allocator accepts NULL and the exact allocation length returned by the adapter.
        unsafe { wb_vt_buffer_free(pointer, length) };
        bytes
    }
    #[cfg(test)]
    fn restore(bytes: &[u8]) -> io::Result<Self> {
        // SAFETY: the decoder reads this slice synchronously and returns a newly owned context.
        NonNull::new(unsafe { wb_vt_restore(bytes.as_ptr(), bytes.len()) })
            .map(|state| Self { state })
            .ok_or_else(failure)
    }
}
impl Drop for Terminal {
    fn drop(&mut self) {
        // SAFETY: this uniquely owned context is released once, after all borrowed operations finish.
        unsafe { wb_vt_free(self.state.as_ptr()) };
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn snapshot_preserves_both_screens_and_unfinished_parser_state() -> io::Result<()> {
        let mut source = Terminal::new()?;
        source.feed(b"PRIMARY\x1b[?1049hALTERNATE\x1b[31", false)?;
        let mut restored = Terminal::restore(&source.export(true)?)?;
        restored.feed(b"m RED\x1b[0m", false)?;
        assert!(String::from_utf8_lossy(&restored.export(false)?).contains("ALTERNATE RED"));
        restored.feed(b"\x1b[?1049l", false)?;
        assert!(String::from_utf8_lossy(&restored.export(false)?).contains("PRIMARY"));
        assert!(Terminal::restore(b"corrupt").is_err());
        Ok(())
    }
    #[test]
    fn detached_queries_and_attached_reply_suppression_share_the_same_state() -> io::Result<()> {
        let mut terminal = Terminal::new()?;
        terminal.resize(120, 40)?;
        assert_eq!(terminal.feed(b"\x1b[5;9H\x1b[6n", true)?, b"\x1b[5;9R");
        assert_eq!(terminal.feed(b"\x1b[18t", true)?, b"\x1b[8;40;120t");
        assert!(terminal.feed(b"\x1b[6n", false)?.is_empty());
        Ok(())
    }
}
