#pragma once
#include "terminal_item.h"
#include <QAccessible>

void installTerminalAccessibility();
void terminalAccessibilityChanged(TerminalItem *, const TerminalItem::AccessibleSnapshot &,
                                 const TerminalItem::AccessibleSnapshot &);
bool terminalAccessibilitySelfTest(TerminalItem *);
