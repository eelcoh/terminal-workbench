//! Last reported lifecycle state, scoped to one foreground agent invocation.
use crate::agent::Agent;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Status {
    Unknown,
    Working,
    Waiting,
    Ready,
}
impl Status {
    pub fn from_tag(tag: u8) -> Option<Self> {
        match tag {
            0 => Some(Self::Unknown),
            1 => Some(Self::Working),
            2 => Some(Self::Waiting),
            3 => Some(Self::Ready),
            _ => None,
        }
    }
    pub fn tag(self) -> u8 {
        match self {
            Self::Unknown => 0,
            Self::Working => 1,
            Self::Waiting => 2,
            Self::Ready => 3,
        }
    }
    fn name(self) -> &'static str {
        match self {
            Self::Unknown => "unknown",
            Self::Working => "working",
            Self::Waiting => "waiting",
            Self::Ready => "ready",
        }
    }
}

pub struct Attention {
    pub agent: Agent,
    pub status: Status,
    group: Option<i32>,
    run: Option<[u8; 16]>,
    sequence: u64,
}
impl Attention {
    pub fn new() -> Self {
        Self {
            agent: Agent::Unknown,
            status: Status::Unknown,
            group: None,
            run: None,
            sequence: 0,
        }
    }
    pub fn sample(&mut self, agent: Agent, group: Option<i32>) -> bool {
        let changed = self.agent != agent || self.group != group;
        if changed {
            self.agent = agent;
            self.group = group;
            self.status = Status::Unknown;
            self.run = None;
            self.sequence = 0;
        }
        changed
    }
    pub fn report(
        &mut self,
        agent: u8,
        group: i32,
        run: [u8; 16],
        sequence: u64,
        status: Status,
    ) -> bool {
        if !(1..=6).contains(&agent)
            || self.agent.tag() != agent
            || self.group != Some(group)
            || sequence == 0
            || sequence <= self.sequence
            || run == [0; 16]
            || self.run.is_some_and(|current| current != run)
        {
            return false;
        }
        self.run = Some(run);
        self.sequence = sequence;
        self.status = status;
        true
    }
    pub fn json(&self) -> String {
        let identity = self.agent.json().trim_end().trim_end_matches('}');
        let source = if self.status == Status::Unknown {
            "unavailable"
        } else {
            "hook"
        };
        format!(
            "{identity},\"status\":\"{}\",\"status_source\":\"{source}\"}}\n",
            self.status.name()
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn reports_are_scoped_ordered_and_cleared_on_new_invocations() {
        let mut state = Attention::new();
        state.sample(Agent::Codex, Some(100));
        assert!(!state.report(2, 100, [1; 16], 1, Status::Working));
        assert!(!state.report(1, 101, [1; 16], 1, Status::Working));
        assert!(state.report(1, 100, [1; 16], 2, Status::Working));
        assert!(state.report(1, 100, [1; 16], 4, Status::Waiting));
        assert!(!state.report(1, 100, [1; 16], 3, Status::Working));
        assert!(!state.report(1, 100, [2; 16], 5, Status::Ready));
        assert_eq!(state.status, Status::Waiting);
        state.sample(Agent::Codex, Some(101));
        assert_eq!(state.status, Status::Unknown);
        assert!(state.report(1, 101, [2; 16], 1, Status::Ready));
        state.sample(Agent::None, Some(90));
        assert_eq!(state.status, Status::Unknown);
        assert!(!state.report(1, 101, [2; 16], 2, Status::Working));
    }
}
