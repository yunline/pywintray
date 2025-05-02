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

#define PWT_VERSION_DEV 1
#define PWT_VERSION_MAJOR 0
#define PWT_VERSION_MINOR 0
#define PWT_VERSION_MICRO 1
#if PWT_VERSION_DEV
#define PWT_VERSION_SUFFIX ".dev"
#else
#define PWT_VERSION_SUFFIX ""
#endif

#define PYWINTRAY_MESSAGE (WM_USER+20)

#define RAISE_WIN32_ERROR(err_code) PyErr_Format(PyExc_OSError, "Win32 Error %lu", err_code)
#define RAISE_LAST_ERROR() RAISE_WIN32_ERROR(GetLastError())

extern HWND message_window;

#define MAINLOOP_RUNNING() (!(!message_window))

// IconHandle start

typedef struct {
    PyObject_HEAD
    HICON icon_handle;
    BOOL need_free;
} IconHandleObject;

extern PyTypeObject IconHandleType;

IconHandleObject *new_icon_handle(HICON icon_handle, BOOL need_free);

// IconHandle end

// TrayIcon start

typedef struct {
    PyObject_HEAD
    UINT id;
    PyObject *tip;
    BOOL hidden;
    BOOL valid;
    IconHandleObject *icon_handle;

    PyObject *mouse_move_callback;
    PyObject *mouse_button_down_callback;
    PyObject *mouse_button_up_callback;
    PyObject *mouse_double_click_callback;
} TrayIconObject;

extern PyTypeObject TrayIconType;

BOOL global_tray_icon_dict_put(TrayIconObject *value);
TrayIconObject *global_tray_icon_dict_get(UINT id);
BOOL global_tray_icon_dict_del(TrayIconObject *value);

BOOL show_icon(TrayIconObject* tray_icon);

// TrayIcon end

// Menu start

extern PyTypeObject *pMenuType;

PyObject* menu_init_subclass(PyObject *self, PyObject *arg);
PyObject* menu_popup(PyObject *self, PyObject *arg);
PyObject* menu_as_tuple(PyObject *self, PyObject *arg);

BOOL menu_subtype_check(PyObject *arg);
PyTypeObject *init_menu_class(PyObject *module);

#define MENU_INIT_SUBCLASS_TMP_NAME "MIS_"
#define MENU_POPUP_TMP_NAME "MP_"
#define MENU_AS_TUPLE_TMP_NAME "MT_"
#define MENU_CAPSULE_NAME "_capsule_"

typedef struct {
    int data;
} MenuInternals;

// Menu end

// MenuItem start

typedef enum {
    MENU_ITEM_TYPE_NULL = 0,
    MENU_ITEM_TYPE_SEPARATOR,
    MENU_ITEM_TYPE_STRING,
    MENU_ITEM_TYPE_CHECK,
    MENU_ITEM_TYPE_SUBMENU,
} MenuItemTypeEnum;

typedef struct {
    PyObject_HEAD
    MenuItemTypeEnum type;
    PyObject *string;
    PyObject *sub;
} MenuItemObject;

extern PyTypeObject MenuItemType;

// MenuItem end

#endif // PYWINTRAY_H