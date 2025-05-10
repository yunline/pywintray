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

// idm start

typedef struct IDManager IDManager;

IDManager *idm_new();
void idm_delete(IDManager *idm);
UINT idm_allocate_id(IDManager *idm, void *data);
void *idm_get_data_by_id(IDManager *idm, UINT id);
BOOL idm_delete_id(IDManager *idm, UINT id);
int idm_next_data(IDManager *idm, Py_ssize_t *ppos, void **pdata);

// idm end

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

BOOL show_icon(TrayIconObject* tray_icon);

extern IDManager *tray_icon_idm;

// TrayIcon end

// Menu start

typedef struct {
    PyTypeObject ob_base;
    PyObject *items_list;
    HMENU handle;
} MenuTypeObject;

extern MenuTypeObject MenuType;

BOOL menu_subtype_check(PyObject *arg);
BOOL init_menu_class(PyObject *module);

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
    UINT id;
    MenuItemTypeEnum type;
    PyObject *string;
    PyObject *sub;
} MenuItemObject;

extern PyTypeObject MenuItemType;

extern IDManager *menu_item_idm;

// MenuItem end

#endif // PYWINTRAY_H
