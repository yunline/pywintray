# type:ignore

import ctypes.wintypes
import threading
import ctypes
import contextlib
import sysconfig
import typing

import pytest
import pywintray

if typing.TYPE_CHECKING:
    from .test_api_stubs import _test_api
else:
    from pywintray import _test_api

NO_GIL = False
if sysconfig.get_config_var("Py_GIL_DISABLED"):
    NO_GIL = True

SKIP_IF_NOGIL = lambda s:pytest.skip(s) if NO_GIL else None

if not NO_GIL:
    import pywinauto

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

def get_current_menu():
    SKIP_IF_NOGIL("skip since pywin32 doesn't support NO_GIL yet")
    app = pywinauto.Application(backend="win32").connect(class_name="#32768")
    return app.window(class_name="#32768")

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

def test_tray_callback():
    tray = pywintray.TrayIcon(pywintray.load_icon("shell32.dll"))

    # set callback mouse_move
    move_callback_called = None
    MOVE = "move"
    @tray.register_callback("mouse_move")
    def cb(_):
        nonlocal move_callback_called
        move_callback_called = MOVE
    
    # set callback mouse_right_double_click
    rdbc_callback_called = None
    RDBC = "rdbc"
    @tray.register_callback("mouse_right_double_click")
    def cb(_):
        nonlocal rdbc_callback_called
        rdbc_callback_called = RDBC
    
    # set callback mouse_mid_button_down
    # then clear the callback
    mbdn_callback_called = None
    MBDN = "lbdn"
    @tray.register_callback("mouse_mid_button_down")
    def cb(_):
        nonlocal mbdn_callback_called
        mbdn_callback_called = MBDN
    tray.register_callback("mouse_mid_button_down")(None)

    # other callbacks should never be called
    never_called = False
    @tray.register_callback("mouse_left_button_up")
    @tray.register_callback("mouse_left_double_click")
    @tray.register_callback("mouse_right_button_down")
    @tray.register_callback("mouse_right_button_up")
    @tray.register_callback("mouse_mid_button_up")
    @tray.register_callback("mouse_mid_double_click")
    def never(_):
        nonlocal never_called
        never_called = True

    with start_tray_loop_thread() as mainloop_thread:
        windows = get_thread_windows(mainloop_thread)
        assert len(windows)==1
        message_window = windows[0]

        internal_id = _test_api.get_internal_id(tray)

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            internal_id, 
            WM_MOUSEMOVE
        )

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            internal_id, 
            WM_RBUTTONDBLCLK
        )

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            internal_id, 
            WM_MBUTTONDOWN
        )

    # callback should be called correctly
    assert move_callback_called is MOVE
    assert rdbc_callback_called is RDBC

    # cleared callback should not be called
    assert mbdn_callback_called is None

    # without posting event
    # other callbacks should never be called
    assert never_called == False

def test_multiple_tray_callback():
    tray1 = pywintray.TrayIcon(pywintray.load_icon("shell32.dll"))
    tray2 = pywintray.TrayIcon(pywintray.load_icon("shell32.dll"))

    # set callback for tray1
    tray1_callback_called = None
    T1 = "tray1"
    @tray1.register_callback("mouse_move")
    def cb(_):
        nonlocal tray1_callback_called
        tray1_callback_called = T1
    
    # set callback for tray2
    tray2_callback_called = None
    T2 = "tray2"
    @tray2.register_callback("mouse_move")
    def cb(_):
        nonlocal tray2_callback_called
        tray2_callback_called = T2
    
    with start_tray_loop_thread() as mainloop_thread:
        windows = get_thread_windows(mainloop_thread)
        assert len(windows)==1
        message_window = windows[0]

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            _test_api.get_internal_id(tray1), 
            WM_MOUSEMOVE
        )

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            _test_api.get_internal_id(tray2), 
            WM_MOUSEMOVE
        )
    
    assert tray1_callback_called is T1
    assert tray2_callback_called is T2

def test_tray_hide_show():
    # should work without mainloop
    tray = pywintray.TrayIcon(pywintray.load_icon("shell32.dll"), hidden=False)    
    assert tray.hidden==False
    tray = pywintray.TrayIcon(pywintray.load_icon("shell32.dll"), hidden=True)
    assert tray.hidden==True

    tray.show()
    assert tray.hidden is False

    tray.hide()
    assert tray.hidden is True

    with start_tray_loop_thread():
        # should work with mainloop
        tray.show()
        assert tray.hidden is False

        tray.hide()
        assert tray.hidden is True

def test_tray_update_icon():
    icon1 = pywintray.load_icon("shell32.dll", index=1)
    icon2 = pywintray.load_icon("shell32.dll", index=2)
    tray = pywintray.TrayIcon(icon1)

    tray.hide()
    tray.icon_handle = icon2
    tray.show()
    tray.icon_handle = icon1

    with start_tray_loop_thread():
        tray.hide()
        tray.icon_handle = icon2
        tray.show()
        tray.icon_handle = icon1

def test_tray_tip():
    tip1 = "awa"
    tip2 = "啊哇啊"
    tray = pywintray.TrayIcon(pywintray.load_icon("shell32.dll"))

    tray.hide()
    tray.tip = tip1
    assert tray.tip == tip1
    tray.show()
    tray.tip = tip2
    assert tray.tip == tip2

    with start_tray_loop_thread():
        tray.hide()
        tray.tip = tip1
        assert tray.tip == tip1
        tray.show()
        tray.tip = tip2
        assert tray.tip == tip2

def test_multi_mainloop():
    with start_tray_loop_thread():
        # call mainloop when mainloop is already running
        # should raise RuntimeError
        with pytest.raises(RuntimeError):
            pywintray.start_tray_loop()

def test_load_icon_large_small():
    large_x = ctypes.windll.user32.GetSystemMetrics(SM_CXICON)
    large_y = ctypes.windll.user32.GetSystemMetrics(SM_CYICON)
    small_x = ctypes.windll.user32.GetSystemMetrics(SM_CXSMICON)
    small_y = ctypes.windll.user32.GetSystemMetrics(SM_CYSMICON)

    if not all((large_x, large_y, small_x, small_y)):
        raise OSError("Unable to get large/small icon size")

    large_size = (large_x, large_y)
    small_size = (small_x, small_y)

    # load ico
    icon = pywintray.load_icon("tests/resources/peppers3-64x64.ico", large=False)
    hicon = _test_api.get_internal_id(icon)
    assert get_icon_size(hicon) == small_size

    icon = pywintray.load_icon("tests/resources/peppers3-64x64.ico", large=True)
    hicon = _test_api.get_internal_id(icon)
    assert get_icon_size(hicon) == large_size

    # load dll
    icon = pywintray.load_icon("shell32.dll", large=False)
    hicon = _test_api.get_internal_id(icon)
    assert get_icon_size(hicon) == small_size

    icon = pywintray.load_icon("shell32.dll", large=True)
    hicon = _test_api.get_internal_id(icon)
    assert get_icon_size(hicon) == large_size

    # load exe
    icon = pywintray.load_icon("explorer.exe", large=False)
    hicon = _test_api.get_internal_id(icon)
    assert get_icon_size(hicon) == small_size

    icon = pywintray.load_icon("explorer.exe", large=True)
    hicon = _test_api.get_internal_id(icon)
    assert get_icon_size(hicon) == large_size

def test_load_icon_index():
    with pytest.raises(OSError):
        pywintray.load_icon("tests/resources/peppers3-64x64.ico", index=1)
    
    pywintray.load_icon("shell32.dll", index=1)
    pywintray.load_icon("explorer.exe", index=1)

def test_icon_handle_free():
    # test part 1
    hicon = ctypes.windll.shell32.ExtractIconW(
        ctypes.windll.kernel32.GetModuleHandleW(0),
        "shell32.dll", 
        0
    )
    assert hicon!=0

    icon = pywintray.IconHandle.from_int(hicon)
    assert _test_api.get_internal_id(icon) == hicon
    del icon

    # the hicon should not be freed
    # therefore CopyIcon should success
    copied = ctypes.windll.user32.CopyIcon(hicon)
    assert copied!=0

    # clean up
    ctypes.windll.user32.DestroyIcon(copied)
    ctypes.windll.user32.DestroyIcon(hicon)

    # test part 2
    icon2 = pywintray.load_icon("shell32.dll")
    hicon = _test_api.get_internal_id(icon2)
    del icon2

    assert hicon!=0

    # the hicon should be freed automatically
    # therefore CopyIcon should fail
    copied = ctypes.windll.user32.CopyIcon(hicon)
    assert copied==0

def test_menu_multi_bases():
    class A:
        pass

    class Menu1(A, pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1")
        item2 = pywintray.MenuItem.string("item2")
        item3 = pywintray.MenuItem.string("item3")
    with popup_in_new_thread(Menu1):
        handle = Menu1._internal_handle
        assert get_menu_item_string(handle, 0) == "item1"
        assert get_menu_item_string(handle, 1) == "item2"
        assert get_menu_item_string(handle, 2) == "item3"
    
    class Menu2(pywintray.Menu, A):
        item1 = pywintray.MenuItem.string("item1")
        item2 = pywintray.MenuItem.string("item2")
        item3 = pywintray.MenuItem.string("item3")
    with popup_in_new_thread(Menu2):
        handle = Menu2._internal_handle
        assert get_menu_item_string(handle, 0) == "item1"
        assert get_menu_item_string(handle, 1) == "item2"
        assert get_menu_item_string(handle, 2) == "item3"

def test_menu_dealloc():
    class Sub(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1")
        item2 = pywintray.MenuItem.string("item2")
        item3 = pywintray.MenuItem.string("item3")
    
    class Menu1(pywintray.Menu):
        sub1 = pywintray.MenuItem.submenu("sub1")(Sub)

    class Menu2(pywintray.Menu):
        sub1 = pywintray.MenuItem.submenu("sub1")(Sub)
    
    del Menu1

    # deleting Menu1 should not affect Sub and Menu2 
    
    with popup_in_new_thread(Sub):
        handle = Sub._internal_handle
        assert get_menu_item_string(handle, 0) == "item1"
        assert get_menu_item_string(handle, 1) == "item2"
        assert get_menu_item_string(handle, 2) == "item3"
    with popup_in_new_thread(Menu2):
        handle = Menu2._internal_handle
        assert get_menu_item_string(handle, 0) == "sub1"

def test_menu_insert_append_remove():
    class MyMenu(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1")
        item2 = pywintray.MenuItem.string("item2")
        item3 = pywintray.MenuItem.string("item3")
    item4 = pywintray.MenuItem.string("item4")
    item5 = pywintray.MenuItem.string("item5")

    MyMenu.insert_item(0, item4)
    MyMenu.append_item(item5)
    MyMenu.remove_item(1)

    item_tuple = MyMenu.as_tuple()
    assert len(item_tuple) == 4
    assert item_tuple[0] is item4
    assert item_tuple[1] is MyMenu.item2
    assert item_tuple[2] is MyMenu.item3
    assert item_tuple[3] is item5

    with popup_in_new_thread(MyMenu):
        handle = MyMenu._internal_handle
        assert get_menu_item_count(handle)==4
        assert get_menu_item_string(handle, 0) == "item4"
        assert get_menu_item_string(handle, 1) == "item2"
        assert get_menu_item_string(handle, 2) == "item3"
        assert get_menu_item_string(handle, 3) == "item5"

def test_menu_insert_append_remove_when_poped_up():
    class MyMenu(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1")
        item2 = pywintray.MenuItem.string("item2")
        item3 = pywintray.MenuItem.string("item3")
    item4 = pywintray.MenuItem.string("item4")
    item5 = pywintray.MenuItem.string("item5")
    
    with popup_in_new_thread(MyMenu):
        MyMenu.insert_item(0, item4)
        MyMenu.append_item(item5)
        MyMenu.remove_item(1)

        item_tuple = MyMenu.as_tuple()
        assert len(item_tuple) == 4
        assert item_tuple[0] is item4
        assert item_tuple[1] is MyMenu.item2
        assert item_tuple[2] is MyMenu.item3
        assert item_tuple[3] is item5

        handle = MyMenu._internal_handle
        assert get_menu_item_count(handle)==4
        assert get_menu_item_string(handle, 0) == "item4"
        assert get_menu_item_string(handle, 1) == "item2"
        assert get_menu_item_string(handle, 2) == "item3"
        assert get_menu_item_string(handle, 3) == "item5"

def test_menu_insert_remove_negative_index():
    class MyMenu(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1")
        item2 = pywintray.MenuItem.string("item2")
        item3 = pywintray.MenuItem.string("item3")
    item4 = pywintray.MenuItem.string("item4")
    
    MyMenu.remove_item(-2)
    MyMenu.insert_item(-1, item4)

    item_tuple = MyMenu.as_tuple()
    assert len(item_tuple) == 3
    assert item_tuple[0] is MyMenu.item1
    assert item_tuple[1] is item4
    assert item_tuple[2] is MyMenu.item3

def test_menu_callbacks():
    SLOT1 = None
    SLOT2 = None

    def cb1(_):
        nonlocal SLOT1
        SLOT1 = 1
    
    def cb2(_):
        nonlocal SLOT2
        SLOT2 = 2

    class MyMenu(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1", callback=cb1)
        item2 = pywintray.MenuItem.string("item2", callback=cb2)
    
    with popup_in_new_thread(MyMenu):
        menu = get_current_menu()
        menu.menu_item("item2").click_input()
    
    assert SLOT1 is None
    assert SLOT2 == 2

def test_submenu_callbacks():
    SLOT1 = None
    SLOT2 = None

    def cb1(_):
        nonlocal SLOT1
        SLOT1 = 1
    
    def cb2(_):
        nonlocal SLOT2
        SLOT2 = 2

    class MyMenu(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1", callback=cb1)

        @pywintray.MenuItem.submenu("sub1")
        class Sub1(pywintray.Menu):
            item2 = pywintray.MenuItem.string("item2", callback=cb2)
    
    with popup_in_new_thread(MyMenu):
        menu = get_current_menu()
        menu.menu_item("sub1").sub_menu().item("item2").click_input()
    
    assert SLOT1 is None
    assert SLOT2 == 2

def test_submenu_update():
    class MyMenu(pywintray.Menu):
        @pywintray.MenuItem.submenu("sub1")
        class Sub(pywintray.Menu):
            @pywintray.MenuItem.submenu("sub2")
            class SubSub(pywintray.Menu):
                item1 = pywintray.MenuItem.string("item1")
    
    MyMenu.Sub.sub.SubSub.sub.item1.label = "qwerty"

    with popup_in_new_thread(MyMenu):
        handle = MyMenu.Sub.sub.SubSub.sub._internal_handle
        assert get_menu_item_string(handle, 0)=="qwerty"

def test_submenu_partial_init():
    deco = pywintray.MenuItem.submenu("sub")
    partial_item = deco.__self__
    with pytest.raises(ValueError):
        class MyMenu(pywintray.Menu):
            pi = partial_item

def test_submenu_multi_init():
    deco = pywintray.MenuItem.submenu("sub")
    class Menu1(pywintray.Menu):
        pass
    class Menu2(pywintray.Menu):
        pass
    deco(Menu1)
    with pytest.raises(RuntimeError):
        deco(Menu2)

def test_submenu_circular_reference():
    class Menu1(pywintray.Menu):
        pass
    class Menu2(pywintray.Menu):
        sub1 = pywintray.MenuItem.submenu("sub1")(Menu1)
    sub2 = pywintray.MenuItem.submenu("sub2")(Menu2)
    with pytest.raises(ValueError):
        Menu1.append_item(sub2)
    with pytest.raises(ValueError):
        Menu1.insert_item(0, sub2)

def test_popup_position():
    class MyMenu(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1")
        item2 = pywintray.MenuItem.string("item2")
        item3 = pywintray.MenuItem.string("item3")
    
    with popup_in_new_thread(MyMenu, position=(300, 400)):
        menu = get_current_menu()
        rect = menu.rectangle()
        assert rect.left==300
        assert rect.top==400

def test_popup_align():
    class MyMenu(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1")
        item2 = pywintray.MenuItem.string("item2")
        item3 = pywintray.MenuItem.string("item3")
    
    with popup_in_new_thread(MyMenu, position=(300, 400), horizontal_align="center"):
        menu = get_current_menu()
        rect = menu.rectangle()
        assert (rect.left+rect.right)//2==300
    
    with popup_in_new_thread(MyMenu, position=(300, 400), horizontal_align="right"):
        menu = get_current_menu()
        rect = menu.rectangle()
        assert rect.right==300
    
    with popup_in_new_thread(MyMenu, position=(300, 400), vertical_align="center"):
        menu = get_current_menu()
        rect = menu.rectangle()
        assert (rect.top+rect.bottom)//2==400
    
    with popup_in_new_thread(MyMenu, position=(300, 400), vertical_align="bottom"):
        menu = get_current_menu()
        rect = menu.rectangle()
        assert rect.bottom==400

def test_popup_allow_right_click():
    SKIP_IF_NOGIL("skip since pywin32 doesn't support NO_GIL yet")
    from pywinauto.mouse import right_click

    CB_CALLED = None
    def cb(_):
        nonlocal CB_CALLED
        CB_CALLED = "called"
    class MyMenu(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1", callback=cb)
    
    with popup_in_new_thread(MyMenu, allow_right_click=False):
        menu = get_current_menu()
        rect = menu.menu_item("item1").rectangle()
        x = (rect.left + rect.right) // 2
        y = (rect.top + rect.bottom) // 2
        right_click(coords=(x, y))
    
    assert CB_CALLED is None

    with popup_in_new_thread(MyMenu, allow_right_click=True):
        menu = get_current_menu()
        rect = menu.menu_item("item1").rectangle()
        x = (rect.left + rect.right) // 2
        y = (rect.top + rect.bottom) // 2
        right_click(coords=(x, y))
    
    assert CB_CALLED=="called"
