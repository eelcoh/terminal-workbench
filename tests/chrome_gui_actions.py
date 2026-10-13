"""Shared input helpers for native chrome. All events go through the real GUI."""
import time

def action(command,window,title):
    command("windowraise",window);command("windowfocus",window);time.sleep(.5)
    def key(name):
        command('keydown','--window',window,name);time.sleep(.1)
        command('keyup','--window',window,name);time.sleep(.08)
    key('ctrl+shift+p')
    command('type','--clearmodifiers','--window',window,'--delay',15,'--',title)
    key('Return')

def workspace(click,index):
    click(22,66)
    click(100,117+index*52)

def position(index,count,width=1240):
    return width/2-max(28,count*28)/2+index*28+14
