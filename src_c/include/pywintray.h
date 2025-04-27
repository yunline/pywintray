#ifndef PYWINTRAY_H
#define PYWINTRAY_H

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <Windows.h>
#include <shellapi.h>
#include <Python.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

#define PYWINTRAY_MESSAGE (WM_USER+20)

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

extern PyTypeObject IconHandleType;

typedef struct {
    PyObject_HEAD
    UINT id;
    PyObject *tip;
    BOOL hidden;
    BOOL destroyed;
    IconHandleObject *icon_handle;

    PyObject *mouse_move_callback;
    PyObject *mouse_button_down_callback;
    PyObject *mouse_button_up_callback;
    PyObject *mouse_double_click_callback;
} TrayIconObject;

extern PyTypeObject TrayIconType;


extern PyObject *global_tray_icon_dict;

IconHandleObject *new_icon_handle(HICON icon_handle, BOOL need_free);

BOOL global_tray_icon_dict_put(TrayIconObject *value);
TrayIconObject *global_tray_icon_dict_get(UINT id);
BOOL global_tray_icon_dict_del(TrayIconObject *value);

BOOL show_icon(TrayIconObject* tray_icon);


#endif // PYWINTRAY_H