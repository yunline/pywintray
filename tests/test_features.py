# type:ignore

import ctypes.wintypes
import threading
import ctypes
import contextlib
import sys

import pytest
import pywintray

# win32 constants
WM_USER = 0x0400
WM_MOUSEMOVE = 0x0200
WM_RBUTTONDBLCLK = 0x0206
WM_MBUTTONDOWN = 0x0207

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
def start_mainloop_thread():
    mainloop_thread = threading.Thread(
        target=pywintray.mainloop,
        daemon=True
    )
    mainloop_thread.start()
    try:
        yield mainloop_thread
    finally:
        pywintray.quit()
        mainloop_thread.join(2)
        if mainloop_thread.is_alive():
            pytest.exit("timeout quitting the mainloop thread", 1)

@contextlib.contextmanager
def popup_in_new_thread(menu:type[pywintray.Menu], *args, **kwargs):
    popup_thread = threading.Thread(
        target=menu.popup,
        args=args,
        kwargs=kwargs,
        daemon=True
    )
    popup_thread.start()
    try:
        yield popup_thread
    finally:
        menu.close()
        popup_thread.join(2)
        if popup_thread.is_alive():
            pytest.exit("timeout quitting the popup thread", 1)

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

    with start_mainloop_thread() as mainloop_thread:
        windows = get_thread_windows(mainloop_thread)
        assert len(windows)==1
        message_window = windows[0]

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            tray._internal_id, 
            WM_MOUSEMOVE
        )

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            tray._internal_id, 
            WM_RBUTTONDBLCLK
        )

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            tray._internal_id, 
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
    
    with start_mainloop_thread() as mainloop_thread:
        windows = get_thread_windows(mainloop_thread)
        assert len(windows)==1
        message_window = windows[0]

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            tray1._internal_id, 
            WM_MOUSEMOVE
        )

        ctypes.windll.user32.PostMessageW(
            message_window, 
            PYWINTRAY_MESSAGE, 
            tray2._internal_id, 
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

    with start_mainloop_thread():
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
    tray.update_icon(icon2)
    tray.show()
    tray.update_icon(icon1)

    with start_mainloop_thread():
        tray.hide()
        tray.update_icon(icon2)
        tray.show()
        tray.update_icon(icon1)

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

    with start_mainloop_thread():
        tray.hide()
        tray.tip = tip1
        assert tray.tip == tip1
        tray.show()
        tray.tip = tip2
        assert tray.tip == tip2

def test_multi_mainloop():
    with start_mainloop_thread():
        # call mainloop when mainloop is already running
        # should raise RuntimeError
        with pytest.raises(RuntimeError):
            pywintray.mainloop()

def test_icon_handle_free():
    # test part 1
    hicon = ctypes.windll.shell32.ExtractIconW(
        ctypes.windll.kernel32.GetModuleHandleW(0),
        "shell32.dll", 
        0
    )
    assert hicon!=0

    icon = pywintray.IconHandle.from_int(hicon)
    assert icon.value == hicon
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
    hicon = icon2.value
    del icon2

    assert hicon!=0

    # the hicon should be freed automatically
    # therefore CopyIcon should fail
    copied = ctypes.windll.user32.CopyIcon(hicon)
    assert copied==0

def test_menu_creation():
    class MyMenu(pywintray.Menu):
        item1 = pywintray.MenuItem.string("item1")
        item2 = pywintray.MenuItem.string("item2")
        item3 = pywintray.MenuItem.string("item3")
    
    item_tuple = MyMenu.as_tuple()
    assert len(item_tuple) == 3
    assert item_tuple[0] is MyMenu.item1
    assert item_tuple[1] is MyMenu.item2
    assert item_tuple[2] is MyMenu.item3

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

def test_menu_property_poped_up():
    class MyMenu(pywintray.Menu):
        pass
    assert MyMenu.poped_up == False
    with popup_in_new_thread(MyMenu):
        assert MyMenu.poped_up == True
    assert MyMenu.poped_up == False
