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
extern BOOL message_window_class_registered;
extern HANDLE hInstance;

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