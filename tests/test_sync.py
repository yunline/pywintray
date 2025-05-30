# type:ignore

import threading

import pytest
import pywintray
from pywintray import _test_api

def test_tray_icon_multithread_id_allocation():
    icon = pywintray.load_icon("shell32.dll")

    tray = pywintray.TrayIcon(icon)
    base = _test_api.get_internal_id(tray)

    _internal_dict = _test_api.get_internal_tray_icon_dict()
    dict_length_base = len(_internal_dict)

    N = 1000
    def run(l:list):
        for i in range(N):
            l.append(pywintray.TrayIcon(icon))
    l1 = []
    l2 = []
    t1 = threading.Thread(target=run, args=(l1,))
    t2 = threading.Thread(target=run, args=(l2,))

    t1.start()
    t2.start()

    t1.join(2)
    t2.join(2)
    
    if t1.is_alive() or t2.is_alive():
        pytest.exit("timeout quitting t1 or t2", 1)

    _internal_dict = _test_api.get_internal_tray_icon_dict()
    assert len(_internal_dict) == dict_length_base + 2*N

    tray = pywintray.TrayIcon(icon)
    assert _test_api.get_internal_id(tray) == (base + 2*N + 1)
    
    
