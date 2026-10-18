//! Bound pending PTY input by bytes without treating a short key burst as 16 large pastes.
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::mpsc::{self, Receiver, SyncSender};
use std::sync::Arc;

const MAX_BYTES: usize = 1024 * 1024;
const MAX_PACKETS: usize = 4096;

pub(crate) struct PendingInput {
    bytes: Vec<u8>,
    budget: Arc<AtomicUsize>,
}
impl AsRef<[u8]> for PendingInput {
    fn as_ref(&self) -> &[u8] {
        &self.bytes
    }
}
impl Drop for PendingInput {
    fn drop(&mut self) {
        self.budget.fetch_sub(self.bytes.len(), Ordering::Relaxed);
    }
}
pub(crate) struct InputSender {
    sender: SyncSender<PendingInput>,
    budget: Arc<AtomicUsize>,
}
pub(crate) fn channel() -> (InputSender, Receiver<PendingInput>) {
    let (sender, receiver) = mpsc::sync_channel(MAX_PACKETS);
    (
        InputSender {
            sender,
            budget: Arc::new(AtomicUsize::new(0)),
        },
        receiver,
    )
}
impl InputSender {
    pub(crate) fn enqueue(&self, bytes: Vec<u8>) -> bool {
        if bytes.is_empty() {
            return true;
        }
        let length = bytes.len();
        let mut used = self.budget.load(Ordering::Relaxed);
        loop {
            let Some(next) = used.checked_add(length).filter(|next| *next <= MAX_BYTES) else {
                return false;
            };
            match self.budget.compare_exchange_weak(
                used,
                next,
                Ordering::Relaxed,
                Ordering::Relaxed,
            ) {
                Ok(_) => break,
                Err(actual) => used = actual,
            }
        }
        // Budget remains reserved while the PTY writer owns the packet, including
        // a blocked write. Failed enqueue and receiver shutdown release it via Drop.
        self.sender
            .try_send(PendingInput {
                bytes,
                budget: self.budget.clone(),
            })
            .is_ok()
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn short_bursts_preserve_order_with_a_delayed_writer() {
        let (sender, receiver) = channel();
        for i in 0..2048 {
            assert!(sender.enqueue(vec![(i % 251) as u8]));
        }
        assert_eq!(sender.budget.load(Ordering::Relaxed), 2048);
        for i in 0..2048 {
            let packet = receiver.try_recv().unwrap();
            assert_eq!(packet.as_ref(), &[(i % 251) as u8]);
        }
        assert_eq!(sender.budget.load(Ordering::Relaxed), 0);
    }
    #[test]
    fn bytes_are_bounded_until_the_writer_releases_them() {
        let (sender, receiver) = channel();
        assert!(sender.enqueue(vec![1; MAX_BYTES]));
        let writing = receiver.try_recv().unwrap();
        assert!(!sender.enqueue(vec![2]));
        drop(writing);
        assert!(sender.enqueue(vec![3]));
        drop(receiver);
        assert_eq!(sender.budget.load(Ordering::Relaxed), 0);
        assert!(!sender.enqueue(vec![4]));
        assert_eq!(sender.budget.load(Ordering::Relaxed), 0);
        assert!(!sender.enqueue(vec![0; MAX_BYTES + 1]));
    }
    #[test]
    fn message_count_is_bounded_and_failed_sends_return_their_budget() {
        let (sender, receiver) = channel();
        for _ in 0..MAX_PACKETS {
            assert!(sender.enqueue(vec![1]));
        }
        assert!(!sender.enqueue(vec![2]));
        assert_eq!(sender.budget.load(Ordering::Relaxed), MAX_PACKETS);
        drop(receiver);
        assert_eq!(sender.budget.load(Ordering::Relaxed), 0);
    }
}
