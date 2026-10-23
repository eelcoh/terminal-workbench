"""Close an X11 window without a window manager, as Xvfb tests need."""
import ctypes
import ctypes.util


def close_window(window, environment):
    # Xvfb has no window manager. Deliver WM_DELETE_WINDOW directly.
    library = ctypes.CDLL(ctypes.util.find_library("X11"))
    library.XOpenDisplay.argtypes = [ctypes.c_char_p]
    library.XOpenDisplay.restype = ctypes.c_void_p
    library.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    library.XInternAtom.restype = ctypes.c_ulong
    library.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.c_void_p]
    library.XFlush.argtypes = [ctypes.c_void_p]
    library.XCloseDisplay.argtypes = [ctypes.c_void_p]
    class Message(ctypes.Structure):
        _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong),
                    ("send_event", ctypes.c_int), ("display", ctypes.c_void_p),
                    ("window", ctypes.c_ulong), ("message_type", ctypes.c_ulong),
                    ("format", ctypes.c_int), ("data", ctypes.c_long * 5)]
    display = library.XOpenDisplay(environment["DISPLAY"].encode())
    assert display
    message = Message()
    message.type, message.display, message.window, message.format = 33, display, int(window), 32
    message.message_type = library.XInternAtom(display, b"WM_PROTOCOLS", 0)
    message.data[0] = library.XInternAtom(display, b"WM_DELETE_WINDOW", 0)
    assert library.XSendEvent(display, int(window), 0, 0, ctypes.byref(message))
    library.XFlush(display)
    library.XCloseDisplay(display)
