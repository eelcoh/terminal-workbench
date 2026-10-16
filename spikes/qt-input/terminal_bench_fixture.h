#pragma once
#include "terminal_text.h"
// Fixed 80x24 Ghostty-extracted corpus shared by CPU and scene-graph probes.
QVector<QVector<TerminalTextCell>> terminalBenchmarkFixture(bool unicode);
