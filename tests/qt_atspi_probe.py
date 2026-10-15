"""Probe the real Qt AT-SPI bridge on a private dbus-run-session bus."""
import json
from pathlib import Path
import sys
import gi
gi.require_version("Gio","2.0")
from gi.repository import Gio, GLib
from session_smoke import wait_until

session=Gio.bus_get_sync(Gio.BusType.SESSION,None)
address=session.call_sync("org.a11y.Bus","/org/a11y/bus","org.a11y.Bus","GetAddress",None,None,Gio.DBusCallFlags.NONE,5000,None).unpack()[0]
bus=Gio.DBusConnection.new_for_address_sync(address,Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT|Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION,None,None)


def call(name,path,interface,method,args=None):
    return bus.call_sync(name,path,interface,method,args,None,Gio.DBusCallFlags.NONE,3000,None).unpack()


def prop(name,path,interface,property_name):
    return call(name,path,"org.freedesktop.DBus.Properties","Get",GLib.Variant("(ss)",(interface,property_name)))[0]


def find_terminal():
    # Enumerate from the registry root, proving the item is actually discoverable.
    pending=[("org.a11y.atspi.Registry","/org/a11y/atspi/accessible/root")]
    seen=set()
    for _ in range(128):
        if not pending:break
        name,path=pending.pop(0)
        if (name,path) in seen:continue
        seen.add((name,path))
        try:
            title=prop(name,path,"org.a11y.atspi.Accessible","Name")
            if title=="Terminal":return name,path
            pending.extend(call(name,path,"org.a11y.atspi.Accessible","GetChildren")[0])
        except GLib.Error:continue
    return None

name,path=wait_until(find_terminal,timeout=10)
text=call(name,path,"org.a11y.atspi.Text","GetText",GLib.Variant("(ii)",(0,-1)))[0]
assert "QT-SNAPSHOT" in text and "café λ" in text and "漢" in text and "😀" in text,text
assert "history-" not in text
# Screen readers navigate lines, not only a bulk text value.
line,start,end=call(name,path,"org.a11y.atspi.Text","GetStringAtOffset",GLib.Variant("(iu)",(14,3)))
assert line.rstrip("\n")=="café λ" and start==12 and end==19,(line,start,end)
emoji_offset=len(text[:text.index("😀")].encode("utf-16-le"))//2
assert call(name,path,"org.a11y.atspi.Text","GetText",GLib.Variant("(ii)",(emoji_offset,emoji_offset+2)))[0]=="😀"
role=call(name,path,"org.a11y.atspi.Accessible","GetRoleName")[0]
assert role=="terminal",role
interfaces=call(name,path,"org.a11y.atspi.Accessible","GetInterfaces")[0]
assert "org.a11y.atspi.Text" in interfaces and "org.a11y.atspi.EditableText" not in interfaces,interfaces
count=prop(name,path,"org.a11y.atspi.Text","CharacterCount")
# QAccessible's text contract uses UTF-16 offsets; record the bridge result.
assert count==len(text.encode("utf-16-le"))//2,(count,len(text))
cursor=prop(name,path,"org.a11y.atspi.Text","CaretOffset")
assert 0<=cursor<=count,cursor
# Native accessible selection must affect the same VT selection used by Copy.
assert call(name,path,"org.a11y.atspi.Text","AddSelection",GLib.Variant("(ii)",(0,4)))[0]
selection=call(name,path,"org.a11y.atspi.Text","GetSelection",GLib.Variant("(i)",(0,)))
assert selection[:2]==(0,4),selection
assert call(name,path,"org.a11y.atspi.Text","RemoveSelection",GLib.Variant("(i)",(0,)))[0]
actions=call(name,path,"org.a11y.atspi.Action","GetActions")[0]
assert actions,actions
focus_index=next((i for i,entry in enumerate(actions) if entry[0]=="SetFocus" or "focus" in entry[0].lower()),None)
assert focus_index is not None,actions
assert call(name,path,"org.a11y.atspi.Action","DoAction",GLib.Variant("(i)",(focus_index,)))[0]
# The bridge must not let a screen reader move the shell cursor as document text.
call(name,path,"org.a11y.atspi.Text","SetCaretOffset",GLib.Variant("(i)",(0,)))
assert prop(name,path,"org.a11y.atspi.Text","CaretOffset")==cursor
events=[]
def changed(connection,sender,object_path,interface,member,parameters,data):
    events.append(parameters.unpack())
bus.signal_subscribe(name,"org.a11y.atspi.Event.Object","TextChanged",path,None,Gio.DBusSignalFlags.NONE,changed,None)
call("org.a11y.atspi.Registry","/org/a11y/atspi/registry","org.a11y.atspi.Registry","RegisterEvent",GLib.Variant("(sass)",("object:text-changed",[],"")))
# A round trip lets the application's bridge process the listener registration.
call(name,path,"org.a11y.atspi.Text","GetText",GLib.Variant("(ii)",(0,-1)))
Path(sys.argv[2]).touch()
def output_event():
    context=GLib.MainContext.default()
    while context.pending():context.iteration(False)
    return any("LIVE-OUTPUT" in str(event) for event in events)
wait_until(output_event)
updated=call(name,path,"org.a11y.atspi.Text","GetText",GLib.Variant("(ii)",(0,-1)))[0]
assert "LIVE-OUTPUT" in updated
result=dict(line_navigation=True,text_update_event=True,role=role,interfaces=interfaces,character_count=count,caret=cursor,unicode_text=True,selection=True,focus=True)
Path(sys.argv[1]).write_text(json.dumps(result,indent=2))
print("PASS: real AT-SPI discovery, terminal role, Unicode text, selection/focus and live text events; no document editing")
