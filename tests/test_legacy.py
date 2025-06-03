# type:ignore

import sysconfig

import pytest

if sysconfig.get_config_var("Py_GIL_DISABLED"):
    pytest.skip(
        "These tests requires pywinauto which depends on pywin32"
        " which has not support NO_GIL yet",
        allow_module_level=True,
    )

from .utils import *
import pywinauto

def get_current_menu():
    app = pywinauto.Application(backend="win32").connect(class_name="#32768")
    return app.window(class_name="#32768")

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
