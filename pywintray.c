#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE 

#include <Windows.h>
#include <shellapi.h>
#include <Python.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")


#define RAISE_WIN32_ERROR(err_code) PyErr_Format(PyExc_OSError, "Win32 Error %lu", err_code)
#define RAISE_LAST_ERROR() RAISE_WIN32_ERROR(GetLastError())

#define MESSAGE_WINDOW_CLASS_NAME TEXT("PyWinTrayWindowClass")
static HWND message_window = NULL;
static BOOL message_window_class_registered = FALSE;
static HANDLE hInstance = NULL;
static PyObject *pywintray_module_obj = NULL;
static UINT tray_icon_id_counter = 1;
#define CALLBACK_MESSAGE (WM_USER+20)

typedef struct {
    PyObject_HEAD
    UINT id;
    PyObject *tip;

} TrayIconObject;

static BOOL init_message_window();
static BOOL deinit_message_window();
static LRESULT window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return TRUE;
    UNREFERENCED_PARAMETER(fdwReason);
    UNREFERENCED_PARAMETER(hinstDLL);
    UNREFERENCED_PARAMETER(lpvReserved);
}

static void
tray_icon_dealloc(TrayIconObject *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
tray_icon_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    TrayIconObject *self;
    self = (TrayIconObject *)type->tp_alloc(type, 0);
    return (PyObject *)self;
}

void init_notify_data(TrayIconObject* tray_icon, NOTIFYICONDATAW *notify_data) {
    notify_data->cbSize = sizeof(NOTIFYICONDATAW);
    notify_data->hWnd = message_window;
    notify_data->uID = tray_icon->id;
    notify_data->hIcon = NULL;
    notify_data->dwState = NIS_SHAREDICON;
}

static int
tray_icon_init(TrayIconObject *self, PyObject *args, PyObject* kwargs)
{
    static char *kwlist[] = {"tip", NULL};

    self->tip = NULL;
    self->id = tray_icon_id_counter;
    tray_icon_id_counter++;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|U", kwlist, &(self->tip))) {
        return -1;
    }

    if(self->tip==NULL) {
        self->tip = Py_BuildValue("s", "pywintray");
    }
    else {
        Py_INCREF(self->tip);
    }

    NOTIFYICONDATAW notify_data;
    init_notify_data(self, &notify_data);
    notify_data.uCallbackMessage = CALLBACK_MESSAGE;
    notify_data.uFlags = NIF_MESSAGE | NIF_TIP; // | NIF_ICON

    if(-1==PyUnicode_AsWideChar(self->tip, notify_data.szTip, sizeof(notify_data.szTip))) {
        Py_DECREF(self->tip);
        return -1;
    }


    if(!Shell_NotifyIcon(NIM_ADD, &notify_data)) {
        Py_DECREF(self->tip);
        RAISE_LAST_ERROR();
        return -1;
    }
    
    return 0;
}

static PyObject *
tray_icon_get_tip(TrayIconObject *self, void *closure) {
    Py_INCREF(self->tip);
    return self->tip;
}

static int
tray_icon_set_tip(TrayIconObject *self, PyObject *value, void *closure) {
    if (!PyUnicode_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'tip' must be a string");
        return -1;
    }

    NOTIFYICONDATAW notify_data;
    init_notify_data(self, &notify_data);
    notify_data.uFlags = NIF_TIP;

    if(-1==PyUnicode_AsWideChar(value, notify_data.szTip, sizeof(notify_data.szTip))) {
        return -1;
    }

    if(!Shell_NotifyIcon(NIM_MODIFY, &notify_data)) {
        RAISE_LAST_ERROR();
        return -1;
    }

    Py_DECREF(self->tip);
    self->tip = value;
    Py_INCREF(value);

    return 0;
}


static PyMethodDef tray_methods[] = {
    {NULL, NULL, 0, NULL}
};

static PyGetSetDef tray_getseters[] = {
    {"tip", (getter)tray_icon_get_tip, (setter)tray_icon_set_tip, NULL, NULL},
    {NULL, NULL, 0, NULL, NULL}
};

static PyTypeObject TrayIconType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "wintray.TrayIcon",
    .tp_doc = NULL,
    .tp_basicsize = sizeof(TrayIconObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = tray_icon_new,
    .tp_init = (initproc)tray_icon_init,
    .tp_dealloc = (destructor)tray_icon_dealloc,
    .tp_methods = tray_methods,
    .tp_getset = tray_getseters,
};

static BOOL
init_message_window() {
    hInstance = GetModuleHandle(NULL);
    if (hInstance==NULL) {
        RAISE_LAST_ERROR();
        return FALSE;
    }

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

static LRESULT window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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


static PyMethodDef pywintray_methods[] = {
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
    if (PyType_Ready(&TrayIconType) < 0)
        return NULL;
    
    pywintray_module_obj = PyModule_Create(&pywintray_module);
    if (pywintray_module_obj == NULL)
        return NULL;
    
    Py_INCREF(&TrayIconType);
    if (PyModule_AddObject(pywintray_module_obj, "TrayIcon", (PyObject *)&TrayIconType) < 0) {
        Py_DECREF(&TrayIconType);
        Py_DECREF(pywintray_module_obj);
        return NULL;
    }

    return pywintray_module_obj;
}
