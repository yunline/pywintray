import ctypes
import ctypes.wintypes
import contextlib
import threading
import time
import warnings

import sysconfig

import pytest

import pywintray

NO_GIL = False
if sysconfig.get_config_var("Py_GIL_DISABLED"):
    NO_GIL = True

SKIP_IF_NOGIL = lambda s:pytest.skip(s) if NO_GIL else None

# win32 constants
WM_USER = 0x0400
WM_MOUSEMOVE = 0x0200
WM_RBUTTONDBLCLK = 0x0206
WM_MBUTTONDOWN = 0x0207

SM_CXICON = 11
SM_CYICON = 12
SM_CXSMICON = 49
SM_CYSMICON = 50
SM_CXVIRTUALSCREEN = 78
SM_CYVIRTUALSCREEN = 79

# pywintray internal constants
MESSAGE_WINDOW_CLASS_NAME = "PyWinTrayWindowClass"
PYWINTRAY_MESSAGE = WM_USER + 20

def get_thread_windows(thread:threading.Thread):
    windows = []
    BUF_LEN = 100
    buf = ctypes.create_unicode_buffer(BUF_LEN)
    @ctypes.WINFUNCTYPE(
        ctypes.wintypes.BOOL, 
        ctypes.wintypes.HWND, 
        ctypes.wintypes.LPARAM
    )
    def enum_callback(hwnd, _):
        ctypes.windll.user32.GetClassNameW(hwnd, buf, BUF_LEN)
        if buf.value!=MESSAGE_WINDOW_CLASS_NAME:
            # filter non-pywintray-window
            return True
        windows.append(hwnd)
        return True
    ctypes.windll.user32.EnumThreadWindows(
        thread.native_id,
        enum_callback,
        0
    )
    return windows

def get_menu_window():
    windows = []
    BUF_LEN = 100
    buf = ctypes.create_unicode_buffer(BUF_LEN)
    @ctypes.WINFUNCTYPE(
        ctypes.wintypes.BOOL, 
        ctypes.wintypes.HWND, 
        ctypes.wintypes.LPARAM
    )
    def enum_callback(hwnd, _):
        ctypes.windll.user32.GetClassNameW(hwnd, buf, BUF_LEN)
        if buf.value!="#32768":
            return True
        windows.append(hwnd)
        return True
    ctypes.windll.user32.EnumWindows(
        enum_callback,
        0
    )
    assert len(windows)==1
    return windows[0]

def get_window_rect(hwnd):
    rect = ctypes.wintypes.RECT()
    result = ctypes.windll.user32.GetWindowRect(hwnd, ctypes.byref(rect))
    if not result:
        raise OSError(f"Unable to get rect for window [{hwnd}]")
    return rect

def _raw_get_menu_item_rect(hmenu, item_index):
    rect = ctypes.wintypes.RECT()
    result = ctypes.windll.user32.GetMenuItemRect(
        0, hmenu, item_index, ctypes.byref(rect)
    )
    if not result:
        raise OSError("Unable to get rect for menu item")
    return rect

class _SpinTimer:
    def __init__(self, timeout):
        self.t = time.time()+timeout
    
    def __call__(self):
        return time.time()<self.t

def get_menu_item_rect(hmenu, item_index, timeout=2):
    """
    wait for _raw_get_menu_item_rect returns a valid value
    """
    timer = _SpinTimer(timeout)
    while timer():
        try:
            rect = _raw_get_menu_item_rect(hmenu, item_index)
            break
        except:
            pass
    else:
        raise RuntimeError("Timeout waiting for menu item rect")
    return rect

_SPI_GETMENUSHOWDELAY = 0x006A
menu_show_delay_ms = ctypes.wintypes.DWORD()
result = ctypes.windll.user32.SystemParametersInfoW(
    _SPI_GETMENUSHOWDELAY, 
    0, 
    ctypes.byref(menu_show_delay_ms), 
    0
)
if result:
    MENU_SHOW_DELAY = menu_show_delay_ms.value / 1000
else:
    warnings.warn("unable to get MENU_SHOW_DELAY, assuming MENU_SHOW_DELAY=0.4")
    MENU_SHOW_DELAY = 0.4
MENU_SHOW_DELAY += 0.1
# clean up
del menu_show_delay_ms, result

SCREEN_X = ctypes.windll.user32.GetSystemMetrics(SM_CXVIRTUALSCREEN)
SCREEN_Y = ctypes.windll.user32.GetSystemMetrics(SM_CYVIRTUALSCREEN)
if SCREEN_X==0 or SCREEN_Y==0:
    warnings.warn("unable to get screen size, assuming 1280, 720")
    SCREEN_X = 1280
    SCREEN_Y = 720
SCREEN_CENTER = (SCREEN_X//2, SCREEN_Y//2)

class _MOUSEINPUT(ctypes.Structure):
    _fields_ = [
        ("dx", ctypes.wintypes.LONG),
        ("dy", ctypes.wintypes.LONG),
        ("mouseData", ctypes.wintypes.DWORD),
        ("dwFlags", ctypes.wintypes.DWORD),
        ("time", ctypes.wintypes.DWORD),
        ("dwExtraInfo", ctypes.c_void_p),
    ]

class _INPUT(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.wintypes.DWORD),
        ("mi", _MOUSEINPUT)
    ]

_INPUT_MOUSE = 0

_MOUSEEVENTF_MOVE = 0x0001
_MOUSEEVENTF_LEFTDOWN = 0x0002
_MOUSEEVENTF_LEFTUP = 0x0004
_MOUSEEVENTF_RIGHTDOWN = 0x0008
_MOUSEEVENTF_RIGHTUP = 0x0010

def mouse_click(button="left"):
    inputs = (_INPUT*2)()
    inputs[0].type = _INPUT_MOUSE
    inputs[1].type = _INPUT_MOUSE
    if button=="left":
        inputs[0].mi.dwFlags = _MOUSEEVENTF_LEFTDOWN
        inputs[1].mi.dwFlags = _MOUSEEVENTF_LEFTUP
    elif button=="right":
        inputs[0].mi.dwFlags = _MOUSEEVENTF_RIGHTDOWN
        inputs[1].mi.dwFlags = _MOUSEEVENTF_RIGHTUP
    else:
        raise RuntimeError("Unknown type of button")

    ctypes.windll.user32.SendInput(2, inputs, ctypes.sizeof(_INPUT))

def set_mouse_pos(pos):
    x, y = pos
    ctypes.windll.user32.SetCursorPos(x, y)

def center_of(rect: ctypes.wintypes.RECT):
    return (rect.left+rect.right)//2, (rect.top+rect.bottom)//2

@contextlib.contextmanager
def start_tray_loop_thread():
    loop_thread = threading.Thread(
        target=pywintray.start_tray_loop,
        daemon=True
    )
    loop_thread.start()
    pywintray.wait_for_tray_loop_ready()
    try:
        yield loop_thread
    finally:
        pywintray.stop_tray_loop()
        loop_thread.join(2)
        if loop_thread.is_alive():
            pytest.exit("timeout quitting the tray loop thread", 1)

@contextlib.contextmanager
def popup_in_new_thread(menu:type[pywintray.Menu], *args, **kwargs):
    popup_thread = threading.Thread(
        target=menu.popup,
        args=args,
        kwargs=kwargs,
        daemon=True
    )
    popup_thread.start()
    menu.wait_for_popup()
    try:
        yield popup_thread
    finally:
        menu.close()
        popup_thread.join(2)
        if popup_thread.is_alive():
            pytest.exit("timeout quitting the popup thread", 1)

def get_menu_item_string(hmenu:int, index: int) -> str:
    MF_BYPOSITION = 0x00000400
    BUF_LEN = 100
    buf = ctypes.create_unicode_buffer(BUF_LEN)
    result = ctypes.windll.user32.GetMenuStringW(
        hmenu,
        index,
        ctypes.byref(buf),
        BUF_LEN,
        MF_BYPOSITION
    )

    if not result:
        raise OSError("Unable to get menu item string")

    return buf.value

def get_menu_item_count(hmenu:int) ->int:
    result = ctypes.windll.user32.GetMenuItemCount(hmenu)
    if result<0:
        raise OSError("Unable to get menu item count")
    return result

class ICONINFO(ctypes.Structure):
    _fields_ = [
        ("fIcon", ctypes.wintypes.BOOL),
        ("xHotspot", ctypes.wintypes.DWORD),
        ("yHotspot", ctypes.wintypes.DWORD),
        ("hbmMask", ctypes.wintypes.HBITMAP),
        ("hbmColor", ctypes.wintypes.HBITMAP),
    ]

class BITMAP(ctypes.Structure):
    _fields_ = [
        ("bmType", ctypes.wintypes.LONG),
        ("bmWidth", ctypes.wintypes.LONG),
        ("bmHeight", ctypes.wintypes.LONG),
        ("bmWidthBytes", ctypes.wintypes.LONG),
        ("bmPlanes", ctypes.wintypes.WORD),
        ("bmBitsPixel", ctypes.wintypes.WORD),
        ("bmBits", ctypes.wintypes.LPVOID),
    ]

def get_icon_size(hicon:int):
    icon_info = ICONINFO()
    result = ctypes.windll.user32.GetIconInfo(hicon, ctypes.byref(icon_info))
    if not result:
        raise OSError("Unable to get icon info")
    
    bm = BITMAP()
    result = ctypes.windll.gdi32.GetObjectW(
        ctypes.wintypes.HANDLE(icon_info.hbmColor), 
        ctypes.sizeof(BITMAP), 
        ctypes.byref(bm)
    )
    ctypes.windll.gdi32.DeleteObject(ctypes.wintypes.HANDLE(icon_info.hbmMask))
    ctypes.windll.gdi32.DeleteObject(ctypes.wintypes.HANDLE(icon_info.hbmColor))
    if not result:
        raise OSError("Unable to get bitmap object")
    
    return (bm.bmWidth, bm.bmHeight)
