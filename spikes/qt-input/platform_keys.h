#pragma once
#include <QKeyEvent>
#include <ghostty/vt.h>

// Qt swaps Control/Meta semantics on Cocoa. Keep that platform detail here.
inline GhosttyMods terminalMods(Qt::KeyboardModifiers mods)
{
    GhosttyMods result = (mods & Qt::ShiftModifier ? GHOSTTY_MODS_SHIFT : 0)
        | (mods & Qt::AltModifier ? GHOSTTY_MODS_ALT : 0);
#ifdef Q_OS_MACOS
    if (mods & Qt::MetaModifier) result |= GHOSTTY_MODS_CTRL;
    if (mods & Qt::ControlModifier) result |= GHOSTTY_MODS_SUPER;
#else
    if (mods & Qt::ControlModifier) result |= GHOSTTY_MODS_CTRL;
    if (mods & Qt::MetaModifier) result |= GHOSTTY_MODS_SUPER;
#endif
    return result;
}
inline bool copyKey(QKeyEvent *e)
{
#ifdef Q_OS_MACOS
    return e->matches(QKeySequence::Copy);
#else
    return e->key() == Qt::Key_C && e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier);
#endif
}
inline bool pasteKey(QKeyEvent *e)
{
#ifdef Q_OS_MACOS
    return e->matches(QKeySequence::Paste);
#else
    return e->key() == Qt::Key_V && e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier);
#endif
}

// Pure policy is testable on either platform. Only exact Option+Left/Right
// uses shell word motion; additional modifiers retain terminal key encoding.
inline const char *macWordMotion(int key,Qt::KeyboardModifiers mods)
{
    if(mods!=Qt::AltModifier)return nullptr;
    if(key==Qt::Key_Left)return "\x1b" "b";
    if(key==Qt::Key_Right)return "\x1b" "f";
    return nullptr;
}
inline const char *platformWordMotion(QKeyEvent *event)
{
#ifdef Q_OS_MACOS
    return macWordMotion(event->key(),event->modifiers());
#else
    Q_UNUSED(event);return nullptr;
#endif
}
