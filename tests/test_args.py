# type:ignore
"""
Test the argument types and return types
"""

import threading

import pytest
import pywintray

def test_version():
    assert isinstance(pywintray.__version__, str)
    assert pywintray.__version__

    assert isinstance(pywintray.VERSION, tuple)
    assert len(pywintray.VERSION)==3
    assert isinstance(pywintray.VERSION[0], int)
    assert isinstance(pywintray.VERSION[1], int)
    assert isinstance(pywintray.VERSION[2], int)

def test_stop_tray_loop():
    assert pywintray.stop_tray_loop() is None
    
    with pytest.raises(TypeError):
        pywintray.stop_tray_loop("arg")

def test_start_tray_loop():
    threading.Timer(0.01, pywintray.stop_tray_loop).start()
    assert pywintray.start_tray_loop() is None
    
    with pytest.raises(TypeError):
        pywintray.start_tray_loop("arg")

def test_wait_for_tray_loop_ready():
    with pytest.raises(TypeError):
        pywintray.wait_for_tray_loop_ready("wrong_type")

    pywintray.wait_for_tray_loop_ready(0.01)
    pywintray.wait_for_tray_loop_ready(-1)
    result = pywintray.wait_for_tray_loop_ready(-1.0)
    assert type(result) is bool

def test_load_icon():
    assert isinstance(pywintray.load_icon("shell32.dll"), pywintray.IconHandle)
    
    with pytest.raises(TypeError):
        pywintray.load_icon(114514)
    
    with pytest.raises(TypeError):
        pywintray.load_icon("shell32.dll", index="wrong type")
    
    pywintray.load_icon("shell32.dll", True, 0)
    pywintray.load_icon(filename="shell32.dll", index=0, large=True)

def test_IconHandle():
    icon = pywintray.load_icon("shell32.dll")

    hicon = 114514
    icon_from_int = pywintray.IconHandle(hicon)
    assert isinstance(icon_from_int, pywintray.IconHandle)

    with pytest.raises(TypeError):
        pywintray.IconHandle(1.14)

    with pytest.raises(ValueError):
        pywintray.IconHandle(0)
    
    with pytest.raises(OverflowError):
        pywintray.IconHandle(1<<65)

    with pytest.raises(TypeError):
        pywintray.IconHandle()


def test_TrayIcon_init():
    icon = pywintray.load_icon("shell32.dll")

    with pytest.raises(TypeError):
        pywintray.TrayIcon()

    with pytest.raises(TypeError):
        pywintray.TrayIcon("wrong_type")

    with pytest.raises(TypeError):
        pywintray.TrayIcon(icon, tip=0)

    tray_icon = pywintray.TrayIcon(icon)
    assert tray_icon.hidden==False
    assert tray_icon.tip=="pywintray"

    tray_icon = pywintray.TrayIcon(icon, "awa", True)
    assert tray_icon.hidden==True
    assert tray_icon.tip=="awa"

    pywintray.TrayIcon(hidden=True, icon_handle=icon, tip="awa")
    assert tray_icon.hidden==True
    assert tray_icon.tip=="awa"

class TestTrayIcon:
    @pytest.fixture(autouse=True)
    def setup(self):
        icon = pywintray.load_icon("shell32.dll")
        self.tray_icon = pywintray.TrayIcon(icon)
    
    def test_property_hidden(self):
        assert isinstance(self.tray_icon.hidden, bool)
        self.tray_icon.hidden = True
        assert self.tray_icon.hidden is True
        self.tray_icon.hidden = False
        assert self.tray_icon.hidden is False
    
    def test_property_tip(self):
        assert isinstance(self.tray_icon.tip, str)

        tip = self.tray_icon.tip

        with pytest.raises(TypeError):
            self.tray_icon.tip = 0
        
        assert self.tray_icon.tip==tip

        self.tray_icon.tip="awa"
        assert self.tray_icon.tip=="awa"
    
    def test_property_icon_handle(self):
        assert isinstance(self.tray_icon.icon_handle, pywintray.IconHandle)
        with pytest.raises(TypeError):
            self.tray_icon.icon_handle = "wrong_type"
        icon = pywintray.load_icon("shell32.dll", index=6)
        self.tray_icon.icon_handle = icon
        assert self.tray_icon.icon_handle is icon

    def test_method_show_hide(self):
        assert self.tray_icon.show() is None
        assert self.tray_icon.hide() is None

        with pytest.raises(TypeError):
            self.tray_icon.show(0)
        
        with pytest.raises(TypeError):
            self.tray_icon.hide(0)

    def test_method_register_callback(self):
        with pytest.raises(TypeError):
            self.tray_icon.register_callback(114514)
        with pytest.raises(TypeError):
            self.tray_icon.register_callback("mouse_move", "wrong_type")
        with pytest.raises(TypeError):
            self.tray_icon.register_callback("mouse_move")("wrong_type")
        with pytest.raises(ValueError):
            self.tray_icon.register_callback("invalid_value")
        with pytest.raises(ValueError):
            self.tray_icon.register_callback("invalid_value", lambda:0)

        self.tray_icon.register_callback("mouse_move", lambda:0)
        self.tray_icon.register_callback("mouse_move", None)

        self.tray_icon.register_callback("mouse_move")(lambda:0)
        self.tray_icon.register_callback("mouse_move")(None)

        cb = lambda:0
        assert self.tray_icon.register_callback("mouse_move")(cb) is cb

        @self.tray_icon.register_callback("mouse_move")
        @self.tray_icon.register_callback("mouse_left_button_down")
        @self.tray_icon.register_callback("mouse_mid_double_click")
        def cb(_):
            pass

    def test_method_notify(self):
        with pytest.raises(TypeError):
            self.tray_icon.notify()
        with pytest.raises(TypeError):
            self.tray_icon.notify("title")
        with pytest.raises(TypeError):
            self.tray_icon.notify(0, "msg")
        with pytest.raises(TypeError):
            self.tray_icon.notify("title", 0)
        with pytest.raises(TypeError):
            self.tray_icon.notify("title", "msg", icon="invalid type")

def test_Menu():
    with pytest.raises(TypeError):
        pywintray.Menu()
    with pytest.raises(TypeError):
        pywintray.Menu.as_tuple()
    with pytest.raises(TypeError):
        pywintray.Menu.popup()
    with pytest.raises(TypeError):
        pywintray.Menu.wait_for_popup(-1)
    with pytest.raises(TypeError):
        pywintray.Menu.close()
    with pytest.raises(TypeError):
        pywintray.Menu.insert_item(0, pywintray.MenuItem.separator())
    with pytest.raises(TypeError):
        pywintray.Menu.append_item(pywintray.MenuItem.separator())
    with pytest.raises(TypeError):
        pywintray.Menu.remove_item(0)
    with pytest.raises(TypeError):
        class MyMenu(pywintray.Menu, wrong_arg=1):
            pass

class TestMenuSubclass:
    @pytest.fixture(autouse=True)
    def setup(self):
        class menu(pywintray.Menu):
            item1 = pywintray.MenuItem.separator()
            item2 = pywintray.MenuItem.separator()
        self.menu = menu
    
    def test_metaclass_setattr(self):
        with pytest.raises(AttributeError):
            self.menu.item1 = 114514
    
    def test_classmethod_popup(self):
        with pytest.raises(TypeError):
            self.menu.popup(position="wrong_type")
        with pytest.raises(TypeError):
            self.menu.popup(position=("114","514"))
        with pytest.raises(TypeError):
            self.menu.popup(position=(114,"514"))
        with pytest.raises(TypeError):
            self.menu.popup(position=("114",514))
        with pytest.raises(TypeError):
            self.menu.popup(position=(114, 514, 1919, 810))
        with pytest.raises(TypeError):
            self.menu.popup(position=tuple())
        
        with pytest.raises(TypeError):
            self.menu.popup(horizontal_align=0.0)
        
        with pytest.raises(TypeError):
            self.menu.popup(vertical_align=0.0)
        
        threading.Timer(0.2, self.menu.close).start()
        assert self.menu.popup() is None
    
    def test_classmethod_wait_for_popup(self):
        with pytest.raises(TypeError):
            self.menu.wait_for_popup("wrong_type")
        
        self.menu.wait_for_popup(-1)
        self.menu.wait_for_popup(0.001)
        result = self.menu.wait_for_popup(-1.0)
        assert type(result) is bool

    def test_classmethod_close(self):
        with pytest.raises(TypeError):
            self.menu.close("wrong_arg")

        assert self.menu.close() is None

    def test_classmethod_as_tuple(self):
        with pytest.raises(TypeError):
            self.menu.as_tuple("wrong_arg")

        assert isinstance(self.menu.as_tuple(), tuple)

    def test_classmethod_insert_item(self):
        new_item = pywintray.MenuItem.separator()
        with pytest.raises(TypeError):
            self.menu.insert_item()
        with pytest.raises(TypeError):
            self.menu.insert_item("too_few_args")
        with pytest.raises(TypeError):
            self.menu.insert_item("wrong_type", new_item)
        with pytest.raises(TypeError):
            self.menu.insert_item(0, "wrong_type")
        
        assert self.menu.insert_item(1, new_item) is None
        assert self.menu.as_tuple()[1] is new_item

        new_item = pywintray.MenuItem.separator()
        self.menu.insert_item(-1, new_item)
        assert self.menu.as_tuple()[-2] is new_item

        new_item = pywintray.MenuItem.separator()
        self.menu.insert_item(1145141919, new_item)
        assert self.menu.as_tuple()[-1] is new_item

        new_item = pywintray.MenuItem.separator()
        self.menu.insert_item(-1145141919, new_item)
        assert self.menu.as_tuple()[0] is new_item
    
    def test_classmethod_append_item(self):
        with pytest.raises(TypeError):
            self.menu.append_item("wrong_type")
        with pytest.raises(TypeError):
            self.menu.append_item()
        
        new_item = pywintray.MenuItem.separator()
        assert self.menu.append_item(new_item) is None
        assert self.menu.as_tuple()[-1] is new_item
    
    def test_classmethod_remove_item(self):
        with pytest.raises(TypeError):
            self.menu.remove_item("wrong_type")
        with pytest.raises(TypeError):
            self.menu.remove_item()
        with pytest.raises(IndexError):
            self.menu.remove_item(114514)
        with pytest.raises(IndexError):
            self.menu.remove_item(-114514)

        assert self.menu.remove_item(0) is None

        self.menu.remove_item(-1)

class TestMenuItem:
    def test_new(self):
        with pytest.raises(TypeError):
            pywintray.MenuItem()

    def test_repr(self):
        string_item = pywintray.MenuItem.string("awa")
        separator_item = pywintray.MenuItem.separator()
        check_item = pywintray.MenuItem.check("awa")
        half_submenu_item = pywintray.MenuItem.submenu("awa").__self__
        @pywintray.MenuItem.submenu("awa")
        class submenu_item(pywintray.Menu):
            pass

        assert repr(string_item)=="<MenuItem.string(label='awa')>"
        assert repr(separator_item)=="<MenuItem.separator()>"
        assert repr(check_item)=="<MenuItem.check(label='awa')>"
        assert repr(half_submenu_item)=="<MenuItem.submenu(<NULL>, label='awa')>"
        assert repr(submenu_item)==f"<MenuItem.submenu({submenu_item.sub}, label='awa')>"

    def test_classmethod_separator(self):
        with pytest.raises(TypeError):
            pywintray.MenuItem.separator("too_many_args")
        assert isinstance(pywintray.MenuItem.separator(), pywintray.MenuItem)

    def test_classmethod_string(self):
        with pytest.raises(TypeError):
            pywintray.MenuItem.string()
        with pytest.raises(TypeError):
            pywintray.MenuItem.string(123456)
        with pytest.raises(TypeError):
            pywintray.MenuItem.string("label", callback="non-callable")
        pywintray.MenuItem.string("label")
        pywintray.MenuItem.string("label", callback=None)
        pywintray.MenuItem.string("label", callback=lambda _:0)
        assert isinstance(pywintray.MenuItem.string("label"), pywintray.MenuItem)
    
    def test_classmethod_check(self):
        with pytest.raises(TypeError):
            pywintray.MenuItem.check()
        with pytest.raises(TypeError):
            pywintray.MenuItem.check(123456)
        with pytest.raises(TypeError):
            pywintray.MenuItem.check("label", callback="non-callable")
        pywintray.MenuItem.check("label")
        pywintray.MenuItem.check("label", checked=True, radio=True)
        pywintray.MenuItem.check("label", callback=None)
        pywintray.MenuItem.check("label", callback=lambda _:0)
        assert isinstance(pywintray.MenuItem.check("label"), pywintray.MenuItem)
    
    def test_classmethod_submenu(self):
        with pytest.raises(TypeError):
            pywintray.MenuItem.submenu()
        with pytest.raises(TypeError):
            pywintray.MenuItem.submenu(123456)

        deco = pywintray.MenuItem.submenu("label")
        assert callable(deco)

        with pytest.raises(TypeError):
            deco()
        with pytest.raises(TypeError):
            deco("wrong_type")
        with pytest.raises(TypeError):
            deco(pywintray.Menu)

        class sub(pywintray.Menu):
            pass

        item = deco(sub)
        assert isinstance(item, pywintray.MenuItem)
    
    def test_accessing_metaclass_methods(self):
        # These methods should be able to be accessed
        # as attributes of MenuItem class
        cls = pywintray.MenuItem
        cls.string
        cls.check
        cls.separator
        cls.submenu

        # However, they should not be able to be accessed
        # as attributes of MenuItem instance
        instance = pywintray.MenuItem.string("awa")
        with pytest.raises(AttributeError):
            instance.string
        with pytest.raises(AttributeError):
            instance.check
        with pytest.raises(AttributeError):
            instance.separator
        with pytest.raises(AttributeError):
            instance.submenu
    
    def test_property_type(self):
        item = pywintray.MenuItem.string("label")
        with pytest.raises(AttributeError):
            item.type = "abcd"
    
    def test_method_register_callback(self):
        item = pywintray.MenuItem.string("label")
        with pytest.raises(TypeError):
            item.register_callback("wrong_type")

class TestMenuItemSeparator:
    @pytest.fixture(autouse=True)
    def setup(self):
        self.item = pywintray.MenuItem.separator()
    
    def test_method_register_callback(self):
        with pytest.raises(TypeError):
            self.item.register_callback(lambda:0)

    def test_property_sub(self):
        with pytest.raises(TypeError):
            getattr(self.item, "sub")
    
    def test_property_label(self):
        with pytest.raises(TypeError):
            getattr(self.item, "label")
        with pytest.raises(TypeError):
            setattr(self.item, "label", "string")
    
    def test_property_checked(self):
        with pytest.raises(TypeError):
            getattr(self.item, "checked")
        with pytest.raises(TypeError):
            setattr(self.item, "checked", True)
    
    def test_property_radio(self):
        with pytest.raises(TypeError):
            getattr(self.item, "radio")
        with pytest.raises(TypeError):
            setattr(self.item, "radio", True)
    
    def test_property_enabled(self):
        with pytest.raises(TypeError):
            getattr(self.item, "enabled")
        with pytest.raises(TypeError):
            setattr(self.item, "enabled", True)

    def test_property_type(self):
        assert self.item.type=="separator"

class TestMenuItemString:
    @pytest.fixture(autouse=True)
    def setup(self):
        self.item = pywintray.MenuItem.string("label")
    
    def test_method_register_callback(self):
        callback = lambda:0
        assert self.item.register_callback(callback) is callback
    
    def test_property_sub(self):
        with pytest.raises(TypeError):
            getattr(self.item, "sub")
    
    def test_property_label(self):
        assert isinstance(self.item.label, str)
        with pytest.raises(TypeError):
            self.item.label = 123456
        self.item.label = "label2"
    
    def test_property_checked(self):
        with pytest.raises(TypeError):
            getattr(self.item, "checked")
        with pytest.raises(TypeError):
            setattr(self.item, "checked", True)
    
    def test_property_radio(self):
        with pytest.raises(TypeError):
            getattr(self.item, "radio")
        with pytest.raises(TypeError):
            setattr(self.item, "radio", True)
    
    def test_property_enabled(self):
        assert isinstance(self.item.enabled, bool)
        self.item.enabled = False
    
    def test_property_type(self):
        assert self.item.type=="string"

class TestMenuItemCheck:
    @pytest.fixture(autouse=True)
    def setup(self):
        self.item = pywintray.MenuItem.check("label")
    
    def test_method_register_callback(self):
        callback = lambda:0
        assert self.item.register_callback(callback) is callback
    
    def test_property_sub(self):
        with pytest.raises(TypeError):
            getattr(self.item, "sub")
    
    def test_property_label(self):
        assert isinstance(self.item.label, str)
        with pytest.raises(TypeError):
            self.item.label = 123456
        self.item.label = "label2"
    
    def test_property_checked(self):
        assert isinstance(self.item.checked, bool)
        self.item.checked = True
    
    def test_property_radio(self):
        assert isinstance(self.item.radio, bool)
        self.item.radio = True
    
    def test_property_enabled(self):
        assert isinstance(self.item.enabled, bool)
        self.item.enabled = False
    
    def test_property_type(self):
        assert self.item.type=="check"

class TestMenuItemSubmenu:
    @pytest.fixture(autouse=True)
    def setup(self):
        class sub(pywintray.Menu):
            pass
        self.sub = sub
        self.item = pywintray.MenuItem.submenu("label")(sub)
    
    def test_method_register_callback(self):
        with pytest.raises(TypeError):
            self.item.register_callback(lambda:0)
    
    def test_property_sub(self):
        with pytest.raises(AttributeError):
            self.item.sub = "read-only"
        assert self.item.sub is self.sub
    
    def test_property_label(self):
        assert isinstance(self.item.label, str)
        with pytest.raises(TypeError):
            self.item.label = 123456
        self.item.label = "label2"
    
    def test_property_checked(self):
        with pytest.raises(TypeError):
            getattr(self.item, "checked")
        with pytest.raises(TypeError):
            setattr(self.item, "checked", True)
    
    def test_property_radio(self):
        with pytest.raises(TypeError):
            getattr(self.item, "radio")
        with pytest.raises(TypeError):
            setattr(self.item, "radio", True)
    
    def test_property_enabled(self):
        assert isinstance(self.item.enabled, bool)
        self.item.enabled = False
    
    def test_property_type(self):
        assert self.item.type=="submenu"
