/*
This file implements the pywintray module
*/

#include "pywintray.h"

#define MESSAGE_WINDOW_CLASS_NAME TEXT("PyWinTrayWindowClass")

HWND message_window = NULL;

static PyObject *global_tray_icon_dict = NULL;
static HANDLE hInstance = NULL;
static ATOM message_window_class_atom = 0;

static LRESULT window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL 
global_tray_icon_dict_put(TrayIconObject *value) {
    PyObject *key_obj = PyLong_FromUnsignedLong(value->id);
    if(key_obj==NULL) {
        return FALSE;
    }
    if (PyDict_SetItem(global_tray_icon_dict, key_obj, (PyObject*)value)<0) {
        return FALSE;
    }
    return TRUE;
}

TrayIconObject *
global_tray_icon_dict_get(UINT id) {
    PyObject *key_obj = PyLong_FromUnsignedLong(id);
    if(key_obj==NULL) {
        return NULL;
    }
    return (TrayIconObject *)PyDict_GetItemWithError(global_tray_icon_dict, key_obj);
}

BOOL 
global_tray_icon_dict_del(TrayIconObject *value) {
    PyObject *key_obj = PyLong_FromUnsignedLong(value->id);
    if(key_obj==NULL) {
        return FALSE;
    }
    if (PyDict_DelItem(global_tray_icon_dict, key_obj)<0) {
        return FALSE;
    }
    return TRUE;
}


static BOOL
init_message_window() {
    message_window = CreateWindowEx(
        0,
        MESSAGE_WINDOW_CLASS_NAME,
        TEXT("WinTrayWindow"),
        WS_OVERLAPPED|WS_SYSMENU,
        0,0, // x, y
        CW_USEDEFAULT, CW_USEDEFAULT, // w, h
        NULL, NULL, hInstance, NULL
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
    wchar_t filename[MAX_PATH];
    BOOL large = TRUE;
    int index = 0;
    UINT result;
    HICON icon_handle = NULL;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U|pi", kwlist, &filename_obj, &large, &index)) {
        return NULL;
    }
    
    if(-1==PyUnicode_AsWideChar(filename_obj, filename, sizeof(filename))) {
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
pywintray_quit(PyObject* self, PyObject* args) {
    if(MAINLOOP_RUNNING()){
        PostMessage(message_window, WM_CLOSE, 0,0);
    }
    
    Py_RETURN_NONE;
}

static PyObject*
pywintray_mainloop(PyObject* self, PyObject* args) {
    MSG msg;
    BOOL result;

    if(MAINLOOP_RUNNING()) {
        PyErr_SetString(PyExc_RuntimeError, "mainloop is already running");
        return NULL;
    }

    if(!init_message_window()) {
        return NULL;
    }

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(global_tray_icon_dict, &pos, &key, &value)) {
        if(!(((TrayIconObject *)value)->hidden)) {
            if (!show_icon((TrayIconObject *)value)) {
                deinit_message_window();
                return NULL;
            }
        }
    }

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

static void
call_button_callback(const char *button_str, PyObject *callback) {
    PyObject *call_arg;
    if (callback) {
        call_arg = Py_BuildValue("s", button_str);
        if (call_arg==NULL) {
            PyErr_Print();
            return;
        }
        if (PyObject_CallOneArg(callback, call_arg)==NULL) {
            PyErr_Print();
        }
    }
}

static LRESULT
window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case PYWINTRAY_MESSAGE:
            PyGILState_STATE gstate = PyGILState_Ensure();

            TrayIconObject* tray_icon = global_tray_icon_dict_get((UINT)wParam);
            if (tray_icon==NULL) {
                if(!PyErr_Occurred()) {
                    PyErr_SetString(PyExc_RuntimeError, "Receiving event from unknown tray icon id");
                }
                PyErr_Print();
                PyGILState_Release(gstate);
                return 0;
            }

            switch (lParam)
            {
                case WM_MOUSEMOVE:
                    if (tray_icon->mouse_move_callback) {
                        if (PyObject_CallNoArgs(tray_icon->mouse_move_callback)==NULL) {
                            PyErr_Print();
                        }
                    }
                    break;
                case WM_LBUTTONDOWN:
                    call_button_callback("left", tray_icon->mouse_button_down_callback);
                    break;
                case WM_RBUTTONDOWN:
                    call_button_callback("right", tray_icon->mouse_button_down_callback);
                    break;
                case WM_MBUTTONDOWN:
                    call_button_callback("mid", tray_icon->mouse_button_down_callback);
                    break;
                case WM_LBUTTONUP:
                    call_button_callback("left", tray_icon->mouse_button_up_callback);
                    break;
                case WM_RBUTTONUP:
                    call_button_callback("right", tray_icon->mouse_button_up_callback);
                    break;
                case WM_MBUTTONUP:
                    call_button_callback("mid", tray_icon->mouse_button_up_callback);
                    break;
                case WM_LBUTTONDBLCLK:
                    call_button_callback("left", tray_icon->mouse_double_click_callback);
                    break;
                case WM_RBUTTONDBLCLK:
                    call_button_callback("right", tray_icon->mouse_double_click_callback);
                    break;
                case WM_MBUTTONDBLCLK:
                    call_button_callback("mid", tray_icon->mouse_double_click_callback);
                    break;
            }
        
            PyGILState_Release(gstate);
            return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static PyMethodDef pywintray_methods[] = {
    {"quit", (PyCFunction)pywintray_quit, METH_NOARGS, NULL},
    {"mainloop", (PyCFunction)pywintray_mainloop, METH_NOARGS, NULL},
    {"load_icon", (PyCFunction)pywintray_load_icon, METH_VARARGS|METH_KEYWORDS, NULL},

    {MENU_INIT_SUBCLASS_TMP_NAME, (PyCFunction)menu_init_subclass, METH_O, NULL},
    {MENU_POPUP_TMP_NAME, (PyCFunction)menu_popup, METH_O, NULL},
    {MENU_AS_TUPLE_TMP_NAME, (PyCFunction)menu_as_tuple, METH_O, NULL},
    {NULL, NULL, 0, NULL}
};

static void
pywintray_free(void *self) {
    if (message_window_class_atom) {
        UnregisterClass(MESSAGE_WINDOW_CLASS_NAME, hInstance);
        message_window_class_atom=0;
    }

    Py_XDECREF(global_tray_icon_dict);
    global_tray_icon_dict = NULL;
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
    #define DONT_CLEAN(v) v=NULL

    hInstance = GetModuleHandle(NULL);
    if (hInstance==NULL) {
        RAISE_LAST_ERROR();
        goto error_clean_up;
    }

    WNDCLASS window_class = {
        .style = CS_VREDRAW|CS_HREDRAW,
        .lpfnWndProc = window_proc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = hInstance,
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

    PyObject *module_obj = NULL;
    PyObject *tmp_menu_type = NULL;
    PyObject *tmp_menu_item_type = NULL;
    PyObject *tmp_tray_icon_type = NULL;
    PyObject *tmp_icon_handle_type = NULL;
    PyObject *tmp_version_str = NULL;
    PyObject *tmp_version_tuple = NULL;

    if (PyType_Ready(&MenuItemType) < 0) {
        goto error_clean_up;
    }
    tmp_menu_item_type = (PyObject *)&MenuItemType;
    Py_INCREF(tmp_menu_item_type);

    if (PyType_Ready(&TrayIconType) < 0) {
        goto error_clean_up;
    }
    tmp_tray_icon_type = (PyObject *)&TrayIconType;
    Py_INCREF(tmp_tray_icon_type);

    if (PyType_Ready(&IconHandleType) < 0) {
        goto error_clean_up;
    }
    tmp_icon_handle_type = (PyObject *)&IconHandleType;
    Py_INCREF(tmp_icon_handle_type);

    global_tray_icon_dict = PyDict_New();
    if(global_tray_icon_dict==NULL) {
        goto error_clean_up;
    }
    
    module_obj = PyModule_Create(&pywintray_module);
    if (module_obj == NULL) {
        goto error_clean_up;
    }

    pMenuType = init_menu_class(module_obj);
    tmp_menu_type = (PyObject *)pMenuType;
    if (!tmp_menu_type) {
        goto error_clean_up;
    }

    if (PyModule_AddObject(module_obj, "MenuItem", tmp_menu_item_type) < 0) {
        goto error_clean_up;
    }
    DONT_CLEAN(tmp_menu_item_type);

    if (PyModule_AddObject(module_obj, "Menu", tmp_menu_type) < 0) {
        goto error_clean_up;
    }
    DONT_CLEAN(tmp_menu_type);
    
    if (PyModule_AddObject(module_obj, "TrayIcon", tmp_tray_icon_type) < 0) {
        goto error_clean_up;
    }
    DONT_CLEAN(tmp_tray_icon_type);
    
    if (PyModule_AddObject(module_obj, "IconHandle", tmp_icon_handle_type) < 0) {
        goto error_clean_up;
    }
    DONT_CLEAN(tmp_icon_handle_type);

    tmp_version_str = PyUnicode_FromFormat(
        "%u.%u.%u%s",
        PWT_VERSION_MAJOR,
        PWT_VERSION_MINOR,
        PWT_VERSION_MICRO,
        PWT_VERSION_SUFFIX
    );
    if(!tmp_version_str) {
        goto error_clean_up;
    }

    if (PyModule_AddObject(module_obj, "__version__", tmp_version_str) < 0) {
        goto error_clean_up;
    }
    DONT_CLEAN(tmp_version_str);

    tmp_version_tuple = Py_BuildValue(
        "(iii)",
        PWT_VERSION_MAJOR,
        PWT_VERSION_MINOR,
        PWT_VERSION_MICRO
    );
    if(!tmp_version_tuple) {
        goto error_clean_up;
    }

    if (PyModule_AddObject(module_obj, "VERSION", tmp_version_tuple) < 0) {
        goto error_clean_up;
    }
    DONT_CLEAN(tmp_version_tuple);

    return module_obj;

error_clean_up:
    Py_XDECREF(tmp_menu_item_type);
    Py_XDECREF(tmp_menu_type);
    Py_XDECREF(tmp_tray_icon_type);
    Py_XDECREF(tmp_icon_handle_type);
    Py_XDECREF(tmp_version_str);
    Py_XDECREF(tmp_version_tuple);
    Py_XDECREF(global_tray_icon_dict);
    Py_XDECREF(module_obj);

    return NULL;

    #undef DONT_CLEAN
}
