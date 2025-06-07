# type:ignore

import time
import threading
import typing

import pytest
import pywintray

if typing.TYPE_CHECKING:
    from .test_api_stubs import _test_api
else:
    from pywintray import _test_api

from .utils import _SpinTimer

@pytest.fixture(autouse=True, scope="module")
def disable_gc_fixture(request):
    import gc
    gc.collect()
    gc.disable()
    yield
    gc.enable()

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
    
def test_menu_item_multithread_id_allocation():
    item = pywintray.MenuItem.string("a")
    base = _test_api.get_internal_id(item)

    _internal_dict = _test_api.get_internal_menu_item_dict()
    dict_length_base = len(_internal_dict)

    N = 1000
    def run(l:list):
        for i in range(N):
            l.append(pywintray.MenuItem.string("a"))
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

    _internal_dict = _test_api.get_internal_menu_item_dict()
    assert len(_internal_dict) == dict_length_base + 2*N

    item = pywintray.MenuItem.string("a")
    assert _test_api.get_internal_id(item) == (base + 2*N + 1)

def test_call_start_tray_loop(request):
    assert threading.active_count()==1

    N = 20
    barrier = threading.Barrier(N)

    def run():
        barrier.wait()
        try:
            pywintray.start_tray_loop()
        except:
            pass
    
    threads = [threading.Thread(target=run, daemon=True) for _ in range(N)]
    for th in threads:
        th.start()

    # There should only 1 thread that can run tray loop successfully.
    # Other threads should end very soon.
    # So if they didn't end, there must be multiple tray loop running
    # which is not expected
    spin_timer = _SpinTimer(0.1)
    while spin_timer():
        if threading.active_count()==2:
            break
    else:
        pytest.exit(f"'{request.node.name}' failed, thread count: {threading.active_count()}", 1)

    # clean up
    if not pywintray.wait_for_tray_loop_ready(2):
        pytest.exit("timeout waiting for the tray loop thread ready", 1)
    pywintray.stop_tray_loop()

    for th in threading.enumerate():
        if th.name == "MainThread":
            continue
        if not th.is_alive():
            continue
        th.join(2)
        if th.is_alive():
            pytest.exit("timeout quitting the tray loop thread", 1)
