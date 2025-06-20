/*
This file implements the pywintray module
*/

#include "pywintray.h"

PWTGlobals pwt_globals;

// fix link error LNK2001: unresolved external symbol _fltused
int _fltused = 1;

static LRESULT CALLBACK
tray_window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static PyObject*
pywintray_load_icon(PyObject* self, PyObject* args, PyObject* kwargs) {
    static char *kwlist[] = {"filename", "large", "index", NULL};
    PyObject* filename_obj = NULL;
    wchar_t *filename;
    BOOL large = TRUE;
    int index = 0;
    UINT result;
    HICON icon_handle = NULL;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U|pi", kwlist, &filename_obj, &large, &index)) {
        return NULL;
    }

    filename = PyUnicode_AsWideCharString(filename_obj, NULL);
    if (!filename) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS;
    if (large){
        result = ExtractIconEx(filename, index, &icon_handle, NULL, 1);
    }
    else {
        result = ExtractIconEx(filename, index, NULL, &icon_handle, 1);
    }
    Py_END_ALLOW_THREADS;

    PyMem_Free(filename);

    if(result==UINT_MAX) {
        RAISE_LAST_ERROR();
        return NULL;
    }
    if(icon_handle==NULL) {
        PyErr_SetString(PyExc_OSError, "Unable to load icon");
        return NULL;
    }

    return (PyObject *)new_icon_handle(icon_handle, TRUE);
}

static PyObject*
pywintray_stop_tray_loop(PyObject* self, PyObject* args) {
    PWT_ENTER_TRAY_WINDOW_CS();
    if(PWT_TRAY_WINDOW_AVAILABLE()){
        PostMessage(pwt_globals.tray_window, PYWINTRAY_TRAY_END_LOOP, 0, 0);
    }
    PWT_LEAVE_TRAY_WINDOW_CS();
    
    Py_RETURN_NONE;
}

static PyObject*
pywintray_start_tray_loop(PyObject* self, PyObject* args) {
    LONG is_running = PWT_SET_ATOMIC(pwt_globals.atomic_tray_loop_started);

    if (is_running) {
        PyErr_SetString(PyExc_RuntimeError, "tray loop is already running");
        return NULL;
    }

    PWT_ENTER_TRAY_WINDOW_CS();

    // create tray window
    pwt_globals.tray_window = CreateWindowEx(
        0,
        PWT_WINDOW_CLASS_NAME,
        TEXT("WinTrayWindow"),
        WS_OVERLAPPED|WS_SYSMENU,
        0,0, // x, y
        CW_USEDEFAULT, CW_USEDEFAULT, // w, h
        NULL, NULL, pwt_dll_hinstance, NULL
    );
    if (!pwt_globals.tray_window) {
        RAISE_LAST_ERROR();
        goto clean_up_level_1;
    }

    // set window proc
    SetLastError(0);
    if (!SetWindowLongPtr(pwt_globals.tray_window, GWLP_WNDPROC, (LONG_PTR)tray_window_proc)) {
        DWORD err_code = GetLastError();
        if (err_code) {
            RAISE_WIN32_ERROR(err_code);
            goto clean_up_level_2;
        }
    }

    // add icons
    idm_enter_critical_section(pwt_globals.tray_icon_idm);
    {
        TrayIconObject *value;
        Py_ssize_t pos = 0;
        while (idm_next(pwt_globals.tray_icon_idm, &pos, NULL, &value)) {
            if(!value) {
                break;
            }
            if (!PWT_ADD_ICON_TO_TRAY((TrayIconObject *)value)) {
                break;
            }
        }
    }
    idm_leave_critical_section(pwt_globals.tray_icon_idm);

    if (PyErr_Occurred()) {
clean_up_level_2:
        DestroyWindow(pwt_globals.tray_window);
        pwt_globals.tray_window = NULL;

clean_up_level_1:
        PWT_LEAVE_TRAY_WINDOW_CS();
        PWT_RESET_ATOMIC(pwt_globals.atomic_tray_loop_started);

        return NULL;
    }

    PWT_LEAVE_TRAY_WINDOW_CS();

    MSG msg;
    BOOL result;
    DWORD error_code = 0;

    // start the loop
    Py_BEGIN_ALLOW_THREADS;
    SetEvent(pwt_globals.tray_loop_ready_event);
    while (1) {
        result = GetMessage(&msg, NULL, 0, 0);
        if (result==-1) {
            error_code = GetLastError();
            break;
        }
        if (result==0) {
            break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    ResetEvent(pwt_globals.tray_loop_ready_event);
    Py_END_ALLOW_THREADS;

    if (result==-1) {
        RAISE_WIN32_ERROR(error_code);
    }

    // clean up

    PWT_ENTER_TRAY_WINDOW_CS();

    // delete icons
    idm_enter_critical_section(pwt_globals.tray_icon_idm);
    {
        TrayIconObject *value;
        Py_ssize_t pos = 0;
        while (idm_next(pwt_globals.tray_icon_idm, &pos, NULL, &value)) {
            if(!value) {
                break;
            }
            if (!PWT_DELETE_ICON_FROM_TRAY((TrayIconObject *)value)) {
                break;
            }
        }
    }
    idm_leave_critical_section(pwt_globals.tray_icon_idm);

    DestroyWindow(pwt_globals.tray_window);
    pwt_globals.tray_window = NULL;
    PWT_LEAVE_TRAY_WINDOW_CS();

    InterlockedExchange(&(pwt_globals.atomic_tray_loop_started), 0);

    if (result==-1) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
pywintray_wait_for_tray_loop_ready(PyObject *self, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"timeout", NULL};

    double timeout = 0.0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|d", kwlist, &timeout)) {
        return NULL;
    }

    DWORD result;

    if (timeout<0.0) {
        result = WaitForSingleObject(pwt_globals.tray_loop_ready_event, 0);
    }
    else if (timeout==0.0) {
        result = WaitForSingleObject(pwt_globals.tray_loop_ready_event, INFINITE);
    }
    else {
        result = WaitForSingleObject(pwt_globals.tray_loop_ready_event, (DWORD)(timeout*1000.0));
    }
    

    if (result==WAIT_OBJECT_0) {
        Py_RETURN_TRUE;
    }
    else if (result==WAIT_TIMEOUT) {
        Py_RETURN_FALSE;
    }
    else {
        RAISE_LAST_ERROR();
        return NULL;
    }

    Py_RETURN_NONE;
}

static LRESULT
handle_tray_message(UINT message, UINT id) {
    PyGILState_STATE gstate = PyGILState_Ensure();
    idm_enter_critical_section(pwt_globals.tray_icon_idm);

    TrayIconObject* tray_icon = (TrayIconObject *)idm_get_data_by_id(pwt_globals.tray_icon_idm, id);
    if (tray_icon==NULL) {
        if(!PyErr_Occurred()) {
            PyErr_SetString(PyExc_RuntimeError, "Receiving event from unknown tray icon id");
        }
        PyErr_Print();
        goto finally;
    }

    uint16_t current_callback_type;

    switch (message) {
        case WM_MOUSEMOVE:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_MOVE;
            break;
        case WM_LBUTTONDOWN:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_LBDOWN;
            break;
        case WM_RBUTTONDOWN:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_RBDOWN;
            break;
        case WM_MBUTTONDOWN:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_MBDOWN;
            break;
        case WM_LBUTTONUP:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_LBUP;
            break;
        case WM_RBUTTONUP:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_RBUP;
            break;
        case WM_MBUTTONUP:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_MBUP;
            break;
        case WM_LBUTTONDBLCLK:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_LBDBC;
            break;
        case WM_RBUTTONDBLCLK:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_RBDBC;
            break;
        case WM_MBUTTONDBLCLK:
            current_callback_type = TRAY_ICON_CALLBACK_MOUSE_MBDBC;
            break;
        case NIN_BALLOONUSERCLICK:
            current_callback_type = TRAY_ICON_CALLBACK_NOTIFICATION_CLICK;
            break;
        case NIN_BALLOONTIMEOUT:
            current_callback_type = TRAY_ICON_CALLBACK_NOTIFICATION_TIMEOUT;
            break;
        default:
            goto finally;
    }
        
    if (tray_icon->callback_flags & (1<<current_callback_type)) {
        PyObject *callback = tray_icon->callbacks[current_callback_type];
        if (callback) {
            if (PyObject_CallOneArg(callback, (PyObject *)tray_icon)==NULL) {
                PyErr_Print();
            }
        }
    }

finally:
    idm_leave_critical_section(pwt_globals.tray_icon_idm);
    PyGILState_Release(gstate);
    return 0;
}

static LRESULT CALLBACK
tray_window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case PYWINTRAY_TRAY_END_LOOP:
            // stop the message loop
            PostQuitMessage(0);
            return 0;
        case PYWINTRAY_TRAY_MESSAGE:
            return handle_tray_message((UINT)lParam, (UINT)wParam);
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static PyMethodDef pywintray_methods[] = {
    {"start_tray_loop", (PyCFunction)pywintray_start_tray_loop, METH_NOARGS, NULL},
    {"stop_tray_loop", (PyCFunction)pywintray_stop_tray_loop, METH_NOARGS, NULL},
    {"load_icon", (PyCFunction)pywintray_load_icon, METH_VARARGS|METH_KEYWORDS, NULL},
    {"wait_for_tray_loop_ready", (PyCFunction)pywintray_wait_for_tray_loop_ready, METH_VARARGS|METH_KEYWORDS, NULL},
    {NULL, NULL, 0, NULL}
};

static void
pywintray_free(void *self) {
    if(pwt_globals.tray_icon_idm) {
        idm_delete(pwt_globals.tray_icon_idm);
        pwt_globals.tray_icon_idm = NULL;
    }

    if(pwt_globals.menu_item_idm) {
        idm_delete(pwt_globals.menu_item_idm);
        pwt_globals.menu_item_idm = NULL;
    }

    if (pwt_globals.active_menus_idm) {
        idm_delete(pwt_globals.active_menus_idm);
        pwt_globals.active_menus_idm = NULL;
    }

    if (pwt_globals.tray_loop_ready_event) {
        CloseHandle(pwt_globals.tray_loop_ready_event);
        pwt_globals.tray_loop_ready_event = NULL;
    }

    DeleteCriticalSection(&(pwt_globals.tray_window_cs));
    DeleteCriticalSection(&(pwt_globals.menu_insert_delete_cs));
}

static PyModuleDef pywintray_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "pywintray.__init__",
    .m_doc = NULL,
    .m_size = -1,
    .m_methods = pywintray_methods,
    .m_free = pywintray_free
};

PyMODINIT_FUNC
PyInit_pywintray(void)
{
    PyObject *module_obj = NULL;

    pwt_globals.tray_icon_idm = NULL;
    pwt_globals.menu_item_idm = NULL;

    pwt_globals.tray_window = NULL;
    pwt_globals.atomic_tray_loop_started = 0;

    module_obj = PyModule_Create(&pywintray_module);
    if (module_obj == NULL) {
        goto error_clean_up;
    }

    pwt_globals.tray_icon_idm = idm_new(TRUE);
    if(!pwt_globals.tray_icon_idm) {
        goto error_clean_up;
    }

    pwt_globals.menu_item_idm = idm_new(TRUE);
    if(!pwt_globals.menu_item_idm) {
        goto error_clean_up;
    }

    pwt_globals.active_menus_idm = idm_new(FALSE);
    if(!pwt_globals.active_menus_idm) {
        goto error_clean_up;
    }

    pwt_globals.tray_loop_ready_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!pwt_globals.tray_loop_ready_event) {
        goto error_clean_up;
    }

    InitializeCriticalSection(&(pwt_globals.tray_window_cs));

    InitializeCriticalSection(&(pwt_globals.menu_insert_delete_cs));

    pwt_globals.MenuType = create_menu_type(module_obj);
    if (PyModule_AddType(module_obj, (PyTypeObject *)(pwt_globals.MenuType)) < 0) {
        goto error_clean_up;
    }
    Py_XDECREF(pwt_globals.MenuType);

    pwt_globals.MenuItemType = create_menu_item_type(module_obj);
    if (PyModule_AddType(module_obj, pwt_globals.MenuItemType) < 0) {
        goto error_clean_up;
    }
    Py_XDECREF(pwt_globals.MenuItemType);

    pwt_globals.TrayIconType = create_tray_icon_type(module_obj);
    if (PyModule_AddType(module_obj, pwt_globals.TrayIconType) < 0) {
        goto error_clean_up;
    }
    Py_XDECREF(pwt_globals.TrayIconType);

    pwt_globals.IconHandleType =  create_icon_handle_type(module_obj);
    if (PyModule_AddType(module_obj, pwt_globals.IconHandleType) < 0) {
        goto error_clean_up;
    }
    Py_XDECREF(pwt_globals.IconHandleType);

    PyObject *version_str = PyUnicode_FromFormat(
        "%u.%u.%u%s",
        PWT_VERSION_MAJOR,
        PWT_VERSION_MINOR,
        PWT_VERSION_MICRO,
        PWT_VERSION_SUFFIX
    );
    if (PyModule_AddObjectRef(module_obj, "__version__", version_str) < 0) {
        goto error_clean_up;
    }
    Py_XDECREF(version_str);

    PyObject *version_tuple = Py_BuildValue(
        "(iii)",
        PWT_VERSION_MAJOR,
        PWT_VERSION_MINOR,
        PWT_VERSION_MICRO
    );
    if (PyModule_AddObjectRef(module_obj, "VERSION", version_tuple) < 0) {
        goto error_clean_up;
    }
    Py_XDECREF(version_tuple);

    PyObject *test_api = create_test_api();
    if (PyModule_AddObjectRef(module_obj, "_test_api", test_api) < 0) {
        goto error_clean_up;
    }
    Py_XDECREF(test_api);

    return module_obj;

error_clean_up:
    Py_XDECREF(module_obj);
    return NULL;
}

HINSTANCE pwt_dll_hinstance;

BOOL WINAPI 
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    static WNDCLASS window_class = {
        .style = CS_VREDRAW|CS_HREDRAW,
        .lpfnWndProc = DefWindowProc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        // .hInstance will be set later in DLL_PROCESS_ATTACH
        .hIcon = NULL,
        .hCursor = NULL,
        .hbrBackground = (HBRUSH)COLOR_WINDOW,
        .lpszMenuName = NULL,
        .lpszClassName = PWT_WINDOW_CLASS_NAME
    };

    static ATOM window_class_atom = 0;

    switch(fdwReason) { 
        case DLL_PROCESS_ATTACH:
            // init pwt_dll_hinstance
            pwt_dll_hinstance = hinstDLL;

            window_class.hInstance = pwt_dll_hinstance;

            // register the window class for this process
            window_class_atom = RegisterClass(&window_class);
            if(!window_class_atom) {
                return FALSE;
            }
            break;
        case DLL_PROCESS_DETACH:
            if (lpvReserved) {
                break;
            }
            // clean up window class
            if (window_class_atom) {
                UnregisterClass(PWT_WINDOW_CLASS_NAME, pwt_dll_hinstance);
            }
            break;
    }
    return TRUE;
}
