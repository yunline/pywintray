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

def test_quit():
    assert pywintray.quit() is None
    
    with pytest.raises(TypeError):
        pywintray.quit("arg")

def test_mainloop():
    threading.Timer(0.01, pywintray.quit).start()
    assert pywintray.mainloop() is None
    
    with pytest.raises(TypeError):
        pywintray.mainloop("arg")

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

    assert isinstance(icon.value, int)
    
    with pytest.raises(AttributeError):
        icon.value = 1

    hicon = 114514
    icon_from_int = pywintray.IconHandle.from_int(hicon)
    assert isinstance(icon_from_int, pywintray.IconHandle)
    assert icon_from_int.value==hicon

    with pytest.raises(TypeError):
        pywintray.IconHandle.from_int(1.14)

    with pytest.raises(ValueError):
        pywintray.IconHandle.from_int(0)
    
    with pytest.raises(OverflowError):
        pywintray.IconHandle.from_int(1<<65)

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

        self.tray_icon.show()
        assert self.tray_icon.hidden is False

        self.tray_icon.hide()
        assert self.tray_icon.hidden is True

        with pytest.raises(AttributeError):
            self.tray_icon.hidden = True
    
    def test_property_tip(self):
        assert isinstance(self.tray_icon.tip, str)

        tip = self.tray_icon.tip

        with pytest.raises(TypeError):
            self.tray_icon.tip = 0
        
        assert self.tray_icon.tip==tip

        self.tray_icon.tip="awa"
        assert self.tray_icon.tip=="awa"

    def test_method_show_hide(self):
        assert self.tray_icon.show() is None
        assert self.tray_icon.hide() is None

        with pytest.raises(TypeError):
            self.tray_icon.show(0)
        
        with pytest.raises(TypeError):
            self.tray_icon.hide(0)
    
    def test_method_destroy(self):
        with pytest.raises(TypeError):
            self.tray_icon.destroy(0)

        assert self.tray_icon.destroy() is None
    
    def test_method_update_icon(self):
        with pytest.raises(TypeError):
            self.tray_icon.update_icon("wrong_type")
        with pytest.raises(TypeError):
            self.tray_icon.update_icon()
        
        icon = pywintray.load_icon("shell32.dll", index=2)
        assert self.tray_icon.update_icon(icon) is None
