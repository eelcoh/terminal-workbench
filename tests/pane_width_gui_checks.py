"""Live width acceptance checks shared with the navigation harness."""
from chrome_gui_actions import position, workspace as chrome_workspace
import struct
import time
from theme_gui_smoke import pixel

def exercise_widths(active, click, key, cmd, window, geometry, capture, colors, verify):
    def columns(): return {p:struct.unpack('HHHH',size)[1] for p,size in geometry().items()}
    def preset(name):
        key('super+alt+p');cmd('type','--clearmodifiers','--window',window,'--delay',15,'--',name);key('Return');time.sleep(.12)
    chrome_workspace(click,1);chrome_workspace(click,0)
    click(position(0,active),20,1.35);before=columns()
    key('super+alt+r');two_thirds=columns();assert two_thirds[0]>before[0]
    assert all(two_thirds[p]==before[p] for p in before if p!=0)
    key('super+alt+r');third=columns();assert third[0]<before[0]
    key('super+alt+f');assert columns()[0]>two_thirds[0]
    key('super+alt+f');assert columns()==third
    preset('Terminal width: one half');assert columns()==before
    if active<3: return
    # Explicit Commands presets work through mouse-accessible palette actions.
    for p in range(3): click(position(p,active),20,.3);preset('Terminal width: one third')
    click(position(0,active),20,.4);thirds=columns();image=capture('three-thirds')
    assert pixel(image,220,700)==colors[0] and pixel(image,600,700)==colors[1] and pixel(image,1000,700)==colors[2]
    click(1000,120,.2);verify(2,'third-terminal-click-input')
    click(position(0,active),20,.4)
    # Drag previews never resize live PTYs. Release commits a snapped width.
    cmd('mousemove','--window',window,444,300);cmd('mousedown',1);time.sleep(.1)
    cmd('mousemove','--window',window,836,300);time.sleep(.2)
    assert columns()==thirds
    cmd('mouseup',1);time.sleep(.3);wide=columns()
    # Neighbor presets stay fixed; fractional pixel rounding can cross one cell boundary.
    assert wide[0]>thirds[0] and all(abs(wide[p]-thirds[p])<=1 for p in thirds if p!=0), (thirds,wide)
    image=capture('two-thirds-and-third')
    assert pixel(image,220,700)==colors[0] and pixel(image,1100,700)==colors[1]
    # Neighbor width and another workspace's geometry stay independent.
    chrome_workspace(click,1);background=columns();chrome_workspace(click,0)
    assert columns()==background
    for p in range(3):
        click(position(p,active),20,.3);verify(p,f'width-restore-focus-{p}');preset('Terminal width: one half')
    # Hidden terminals apply restored preferred geometry only when revealed.
    for p in range(active): click(position(p,active),20,.15)
    click(position(0,active),20,.4);assert columns()==before, (before,columns())
