/*
This file implements the pywintray module
*/

#include "pywintray.h"

#define MESSAGE_WINDOW_CLASS_NAME TEXT("PyWinTrayWindowClass")

HWND message_window = NULL;
uint32_t pywintray_state = 0;

PyObject *global_tray_icon_dict = NULL;

static HANDLE hInstance = NULL;
static PyObject *pywintray_module_obj = NULL;

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

    if (!(pywintray_state&PWT_STATE_MESSAGE_WINDOW_CREATED)){
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
        if(RegisterClass(&window_class)==0) {
            RAISE_LAST_ERROR();
            return FALSE;
        }
        pywintray_state |= PWT_STATE_MESSAGE_WINDOW_CREATED;
    }

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

    if (pywintray_state&PWT_STATE_MESSAGE_WINDOW_CREATED) {
        if(!UnregisterClass(MESSAGE_WINDOW_CLASS_NAME, hInstance)) {
            RAISE_LAST_ERROR();
            return FALSE;
        }
        pywintray_state &= (~PWT_STATE_MESSAGE_WINDOW_CREATED);
    }
    return TRUE;
}

static PyObject*
pywintray_quit(PyObject* self, PyObject* args) {
    if(message_window){
        PostMessage(message_window, WM_CLOSE, 0,0);
    }
    
    Py_RETURN_NONE;
}

static PyObject*
pywintray_mainloop(PyObject* self, PyObject* args) {
    MSG msg;
    BOOL result;

    if(pywintray_state&PWT_STATE_MAINLOOP_STARTED) {
        PyErr_SetString(PyExc_RuntimeError, "mainloop is already running");
        return NULL;
    }

    if(!message_window) {
        if(!init_message_window()) {
            return NULL;
        }
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

    pywintray_state |= PWT_STATE_MAINLOOP_STARTED;

    Py_BEGIN_ALLOW_THREADS;
    while (1) {
        result = GetMessage(&msg, NULL, 0, 0);
        if (result==-1) break;
        if (result==0) break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    Py_END_ALLOW_THREADS;

    pywintray_state &= (~PWT_STATE_MAINLOOP_STARTED);
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
    {NULL, NULL, 0, NULL}
};

static void
pywintray_free(void *self) {
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
    hInstance = GetModuleHandle(NULL);
    if (hInstance==NULL) {
        RAISE_LAST_ERROR();
        return FALSE;
    }

    if (PyType_Ready(&TrayIconType) < 0) {
        return NULL;
    }

    global_tray_icon_dict = PyDict_New();
    if(global_tray_icon_dict==NULL) {
        return NULL;
    }
    
    pywintray_module_obj = PyModule_Create(&pywintray_module);
    if (pywintray_module_obj == NULL) {
        Py_DECREF(global_tray_icon_dict);
        return NULL;
    }
    
    Py_INCREF(&TrayIconType);
    if (PyModule_AddObject(pywintray_module_obj, "TrayIcon", (PyObject *)&TrayIconType) < 0) {
        Py_DECREF(global_tray_icon_dict);
        Py_DECREF(&TrayIconType);
        Py_DECREF(pywintray_module_obj);
        return NULL;
    }

    return pywintray_module_obj;
}
