/*
This file implements the pywintray module
*/

#include "pywintray.h"

HWND message_window = NULL;

static ATOM message_window_class_atom = 0;

static BOOL
init_message_window() {
    message_window = CreateWindowEx(
        0,
        MESSAGE_WINDOW_CLASS_NAME,
        TEXT("WinTrayWindow"),
        WS_OVERLAPPED|WS_SYSMENU,
        0,0, // x, y
        CW_USEDEFAULT, CW_USEDEFAULT, // w, h
        NULL, NULL, GetModuleHandle(NULL), NULL
    );
    if (message_window==NULL) {
        RAISE_LAST_ERROR();
        return FALSE;
    }

    return TRUE;
}

static BOOL
deinit_message_window() {
    if (message_window) {
        if(!DestroyWindow(message_window)) {
            DWORD error = GetLastError();
            if(error!=ERROR_INVALID_WINDOW_HANDLE) {
                RAISE_WIN32_ERROR(error);
                return FALSE;
            }
        }
    }
    message_window = NULL;

    return TRUE;
}

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
    if(MAINLOOP_RUNNING()){
        PostMessage(message_window, WM_CLOSE, 0,0);
    }
    
    Py_RETURN_NONE;
}

static PyObject*
pywintray_start_tray_loop(PyObject* self, PyObject* args) {
    MSG msg;
    BOOL result;

    if(MAINLOOP_RUNNING()) {
        PyErr_SetString(PyExc_RuntimeError, "mainloop is already running");
        return NULL;
    }

    if(!init_message_window()) {
        return NULL;
    }

    TrayIconObject *value;
    Py_ssize_t pos = 0;

    idm_mutex_acquire(tray_icon_idm);
    while (idm_next_data(tray_icon_idm, &pos, &value)) {
        if(!value) {
            return NULL;
        }
        if(!(value->hidden)) {
            if (!show_icon((TrayIconObject *)value)) {
                deinit_message_window();
                return NULL;
            }
        }
    }
    idm_mutex_release(tray_icon_idm);

    Py_BEGIN_ALLOW_THREADS;
    while (1) {
        result = GetMessage(&msg, NULL, 0, 0);
        if (result==-1) break;
        if (result==0) break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    Py_END_ALLOW_THREADS;

    message_window = NULL;

    if (result==-1) {
        RAISE_LAST_ERROR();
        deinit_message_window();
        return NULL;
    }
    if (result==0) {
        if(!deinit_message_window()) {
            return NULL;
        }
    }

    Py_RETURN_NONE;
}

static LRESULT
handle_tray_message(UINT message, UINT id) {
    PyGILState_STATE gstate = PyGILState_Ensure();

    TrayIconObject* tray_icon = (TrayIconObject *)idm_get_data_by_id(tray_icon_idm, id);
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
    PyGILState_Release(gstate);
    return 0;
}

static LRESULT CALLBACK
window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(hWnd!=message_window) {
        goto default_handler;
    }

    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case PYWINTRAY_MESSAGE:
            return handle_tray_message((UINT)lParam, (UINT)wParam);
    }

default_handler:
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static PyMethodDef pywintray_methods[] = {
    {"start_tray_loop", (PyCFunction)pywintray_start_tray_loop, METH_NOARGS, NULL},
    {"stop_tray_loop", (PyCFunction)pywintray_stop_tray_loop, METH_NOARGS, NULL},
    {"load_icon", (PyCFunction)pywintray_load_icon, METH_VARARGS|METH_KEYWORDS, NULL},
    {NULL, NULL, 0, NULL}
};

static void
pywintray_free(void *self) {
    if (message_window_class_atom) {
        UnregisterClass(MESSAGE_WINDOW_CLASS_NAME, GetModuleHandle(NULL));
        message_window_class_atom=0;
    }

    if(tray_icon_idm) {
        idm_delete(tray_icon_idm);
        tray_icon_idm = NULL;
    }

    if(menu_item_idm) {
        idm_delete(menu_item_idm);
        menu_item_idm = NULL;
    }
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

    tray_icon_idm = NULL;
    menu_item_idm = NULL;
    message_window_class_atom = 0;

    module_obj = PyModule_Create(&pywintray_module);
    if (module_obj == NULL) {
        goto error_clean_up;
    }

    WNDCLASS window_class = {
        .style = CS_VREDRAW|CS_HREDRAW,
        .lpfnWndProc = window_proc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = GetModuleHandle(NULL),
        .hIcon = NULL,
        .hCursor = LoadCursor(0, IDC_ARROW),
        .hbrBackground = (HBRUSH)COLOR_WINDOW,
        .lpszMenuName = NULL,
        .lpszClassName = MESSAGE_WINDOW_CLASS_NAME
    };
    message_window_class_atom = RegisterClass(&window_class);
    if(!message_window_class_atom) {
        RAISE_LAST_ERROR();
        goto error_clean_up;
    }

    tray_icon_idm = idm_new();
    if(!tray_icon_idm) {
        goto error_clean_up;
    }

    menu_item_idm = idm_new();
    if(!menu_item_idm) {
        goto error_clean_up;
    }

    if(!init_menu_class(module_obj)) {
        goto error_clean_up;
    }

    if (PyModule_AddType(module_obj, (PyTypeObject *)&MenuType) < 0) {
        goto error_clean_up;
    }

    if (PyModule_AddType(module_obj, &MenuItemType) < 0) {
        goto error_clean_up;
    }

    if (PyModule_AddType(module_obj, &TrayIconType) < 0) {
        goto error_clean_up;
    }

    if (PyModule_AddType(module_obj, &IconHandleType) < 0) {
        goto error_clean_up;
    }

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
