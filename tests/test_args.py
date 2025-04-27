# type:ignore
"""
Test the argument types and return types
"""

import threading

import pytest
import pywintray

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
    
    with pytest.raises(TypeError):
        pywintray.IconHandle()
