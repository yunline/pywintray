# type:ignore

import threading
import typing

import pytest
import pywintray

if typing.TYPE_CHECKING:
    from .test_api_stubs import _test_api
else:
    from pywintray import _test_api

from .utils import _SpinTimer
from .utils import *

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
    spin_timer = _SpinTimer(2)
    while spin_timer():
        if (th_cnt:=threading.active_count())==2:
            break
    else:
        pytest.exit(f"'{request.node.name}' failed, thread count: {th_cnt}", 1)

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

def test_update_tray_icon_while_calling_starting_stoping_tray_loop():
    icon1 = pywintray.load_icon("shell32.dll", index=3)
    icon2 = pywintray.load_icon("shell32.dll", index=4)

    tray = pywintray.TrayIcon(icon1)

    stop_event = threading.Event()

    error_occured = False

    def run_update():
        nonlocal error_occured
        try:
            # loop while tray loop is not ready
            while not stop_event.is_set():
                for _ in range(50):
                    tray.show()
                    tray.tip = "a"
                    tray.icon_handle = icon2
                    tray.tip = "b"
                    tray.icon_handle = icon1
                    tray.hide()
        except:
            error_occured = True
            raise
    
    update_thread = threading.Thread(target=run_update, daemon=True)
    update_thread.start()

    # Start the tray loop, then stop
    with start_tray_loop_thread():
        time.sleep(0)

    # Clean up
    stop_event.set()
    update_thread.join(2)
    if update_thread.is_alive():
        pytest.exit("timeout quitting the update_thread", 1)

    assert not error_occured


def test_menu_multithread_insert_delete():
    class Menu(pywintray.Menu):
        pass
    item = pywintray.MenuItem.string("awa")

    N = 50
    for _ in range(N):
        Menu.insert_item(0, item)

    error_occured = False

    barrier = threading.Barrier(2)
    def add_items():
        nonlocal error_occured
        barrier.wait()
        try:
            for _ in range(N):
                Menu.insert_item(0, item)
        except:
            error_occured = True
            raise
    def remove_items():
        nonlocal error_occured
        barrier.wait()
        try:
            for _ in range(N):
                Menu.remove_item(-1)
        except:
            error_occured = True
            raise
    
    adding_thread = threading.Thread(target=add_items)
    removing_thread = threading.Thread(target=remove_items)

    adding_thread.start()
    removing_thread.start()

    adding_thread.join(2)
    removing_thread.join(2)

    if adding_thread.is_alive() or removing_thread.is_alive():
        pytest.exit("timeout quitting the thread", 1)

    assert not error_occured

