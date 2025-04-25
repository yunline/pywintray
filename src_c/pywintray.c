/*
This file implements the pywintray module
*/

#include "pywintray.h"
#include <shellapi.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

#define MESSAGE_WINDOW_CLASS_NAME TEXT("PyWinTrayWindowClass")

HWND message_window = NULL;
uint32_t pywintray_state = 0;

PyObject *global_tray_icon_dict = NULL;

static HANDLE hInstance = NULL;
static PyObject *pywintray_module_obj = NULL;

static LRESULT window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return TRUE;
    UNREFERENCED_PARAMETER(fdwReason);
    UNREFERENCED_PARAMETER(hinstDLL);
    UNREFERENCED_PARAMETER(lpvReserved);
}

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
        return FALSE;
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
    pywintray_state |= PWT_STATE_MAINLOOP_STARTED;

    if(!message_window) {
        if(!init_message_window()) {
            pywintray_state &= (~PWT_STATE_MAINLOOP_STARTED);
            return NULL;
        }
    }
    Py_BEGIN_ALLOW_THREADS;
    while (1) {
        result = GetMessage(&msg, NULL, 0, 0);
        if (result==-1) {
            Py_BLOCK_THREADS;
            RAISE_LAST_ERROR();
            deinit_message_window();
            pywintray_state &= (~PWT_STATE_MAINLOOP_STARTED);
            return NULL;
        }
        if (result==0)
        {
            Py_BLOCK_THREADS;
            message_window = NULL;
            if(!deinit_message_window()) {
                pywintray_state &= (~PWT_STATE_MAINLOOP_STARTED);
                return NULL;
            }
            pywintray_state &= (~PWT_STATE_MAINLOOP_STARTED);
            Py_RETURN_NONE;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);

    }
    Py_END_ALLOW_THREADS;

    Py_RETURN_NONE;
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

            switch (lParam)
            {
                case WM_MOUSEMOVE:
                    if (tray_icon->mouse_move_callback) {
                        PyObject_CallNoArgs(tray_icon->mouse_move_callback);
                    }
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
    .m_name = "pywintray",
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
