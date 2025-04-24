#ifndef PYWINTRAY_H
#define PYWINTRAY_H

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <Windows.h>
#include <Python.h>

#define CALLBACK_MESSAGE (WM_USER+20)

#define RAISE_WIN32_ERROR(err_code) PyErr_Format(PyExc_OSError, "Win32 Error %lu", err_code)
#define RAISE_LAST_ERROR() RAISE_WIN32_ERROR(GetLastError())

extern HWND message_window;
extern uint32_t pywintray_state;
#define PWT_STATE_MESSAGE_WINDOW_CREATED (1<<0)
#define PWT_STATE_MAINLOOP_STARTED (1<<1)

typedef struct {
    PyObject_HEAD
    HICON icon_handle;
    BOOL need_free;
} IconHandleObject;

IconHandleObject *new_icon_handle(HICON icon_handle, BOOL need_free);

extern PyTypeObject IconHandleType;

typedef struct {
    PyObject_HEAD
    UINT id;
    PyObject *tip;
    BOOL hidden;
    IconHandleObject *icon_handle;
} TrayIconObject;

extern PyTypeObject TrayIconType;


#endif // PYWINTRAY_H