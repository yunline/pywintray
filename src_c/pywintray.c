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
BOOL message_window_class_registered = FALSE;
HANDLE hInstance = NULL;

static PyObject *pywintray_module_obj = NULL;

static LRESULT window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return TRUE;
    UNREFERENCED_PARAMETER(fdwReason);
    UNREFERENCED_PARAMETER(hinstDLL);
    UNREFERENCED_PARAMETER(lpvReserved);
}

static BOOL
init_message_window() {

    if (!message_window_class_registered){
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
        message_window_class_registered = TRUE;
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

    if (message_window_class_registered) {
        if(!UnregisterClass(MESSAGE_WINDOW_CLASS_NAME, hInstance)) {
            RAISE_LAST_ERROR();
            return FALSE;
        }
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
    if(!message_window) {
        if(!init_message_window()) {
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
            return NULL;
        }
        if (result==0)
        {
            Py_BLOCK_THREADS;
            message_window = NULL;
            if(!deinit_message_window()) {
                return NULL;
            }
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
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
            break;
    }
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


static PyMethodDef pywintray_methods[] = {
    {"load_icon", (PyCFunction)pywintray_load_icon, METH_VARARGS|METH_KEYWORDS, NULL},
    {"quit", (PyCFunction)pywintray_quit, METH_NOARGS, NULL},
    {"mainloop", (PyCFunction)pywintray_mainloop, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef pywintray_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "pywintray",
    .m_doc = NULL,
    .m_size = -1,
    .m_methods = pywintray_methods,
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
    
    if (PyType_Ready(&IconHandleType) < 0) {
        return NULL;
    }
    
    pywintray_module_obj = PyModule_Create(&pywintray_module);
    if (pywintray_module_obj == NULL)
        return NULL;
    
    Py_INCREF(&TrayIconType);
    if (PyModule_AddObject(pywintray_module_obj, "TrayIcon", (PyObject *)&TrayIconType) < 0) {
        Py_DECREF(&TrayIconType);
        Py_DECREF(pywintray_module_obj);
        return NULL;
    }

    Py_INCREF(&IconHandleType);
    if (PyModule_AddObject(pywintray_module_obj, "IconHandle", (PyObject *)&IconHandleType) < 0) {
        Py_DECREF(&TrayIconType);
        Py_DECREF(&IconHandleType);
        Py_DECREF(pywintray_module_obj);
        return NULL;
    }

    return pywintray_module_obj;
}
