import ctypes
import ctypes.wintypes
import contextlib
import threading

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
