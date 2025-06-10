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

#define PWT_Malloc(sz) PyMem_RawMalloc(sz)
#define PWT_Free(p) PyMem_RawFree(p)

#define PWT_VERSION_DEV 1
#define PWT_VERSION_MAJOR 0
#define PWT_VERSION_MINOR 0
#define PWT_VERSION_MICRO 1
#if PWT_VERSION_DEV
#define PWT_VERSION_SUFFIX ".dev"
#else
#define PWT_VERSION_SUFFIX ""
#endif

#define PYWINTRAY_TRAY_MESSAGE (WM_USER+20)
#define PYWINTRAY_MENU_UPDATE_MESSAGE (WM_USER+21)
#define PYWINTRAY_TRAY_END_LOOP (WM_USER+22)

#define MESSAGE_WINDOW_CLASS_NAME TEXT("PyWinTrayWindowClass")

#define PYWINTRAY_MENU_OBJ_WINDOW_PROP_NAME TEXT("PyWinTrayMenuObject")

#define RAISE_WIN32_ERROR(err_code) raise_win32_error(err_code)
#define RAISE_LAST_ERROR() RAISE_WIN32_ERROR(GetLastError())

inline void raise_win32_error(DWORD error_code) {
    LPWSTR  buffer;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        (LPWSTR)&buffer,
        0,
        NULL
    );

    if (!size) {
        goto fallback;
    }

    PyObject *str_obj = PyUnicode_FromWideChar(buffer, size);
    LocalFree(buffer);
    if (!str_obj) {
        goto fallback;
    }

    PyErr_Format(PyExc_OSError, "Win32 Error %lu: %S", error_code, str_obj);
    Py_DECREF(str_obj);

    return;

fallback:
    PyErr_Format(PyExc_OSError, "Win32 Error %lu", error_code);
    return;
}

// python  api compat
#if PY_VERSION_HEX < 0x030D0000 // version < 3.13

inline int
PyUnicode_EqualToUTF8(PyObject *unicode, const char *string) {
    PyObject *string_obj = PyUnicode_FromString(string);
    if (!string_obj) {
        goto err_clean;
    }
    int result = PyUnicode_Compare(unicode, string_obj);
    Py_DECREF(string_obj);
    if (result==-1 && PyErr_Occurred()) {
        goto err_clean;
    }
    return !result;
err_clean:
    PyErr_Clear();
    return 0;
}

#endif // PY_VERSION_HEX < 0x030D0000

// idm start

typedef enum {
    IDM_FLAGS_NONE = 0,
    IDM_FLAGS_ALLOCATE_ID = 1
} IDMFlags;

typedef struct IDManager IDManager;

IDManager *idm_new(IDMFlags flags);
void idm_delete(IDManager *idm);

void idm_enter_critical_section(IDManager *idm);
void idm_leave_critical_section(IDManager *idm);

// Following functions handles the idm critical section automatically

UINT idm_allocate_id(IDManager *idm, void *data);
BOOL idm_put_id(IDManager *idm, UINT id, void *data);
void *idm_get_data_by_id(IDManager *idm, UINT id);
BOOL idm_delete_id(IDManager *idm, UINT id);

// This function needs to be called in idm critical section
// You need to handle the critical section by your self
int idm_next(IDManager *idm, Py_ssize_t *ppos, UINT *pid, void **pdata);

// idm end

// IconHandle start

typedef struct {
    PyObject_HEAD
    HICON icon_handle;
    BOOL need_free;
} IconHandleObject;

IconHandleObject *new_icon_handle(HICON icon_handle, BOOL need_free);

// IconHandle end

// TrayIcon start

typedef enum {
    TRAY_ICON_CALLBACK_MOUSE_MOVE,
    TRAY_ICON_CALLBACK_MOUSE_LBUP,
    TRAY_ICON_CALLBACK_MOUSE_LBDOWN,
    TRAY_ICON_CALLBACK_MOUSE_LBDBC,
    TRAY_ICON_CALLBACK_MOUSE_RBUP,
    TRAY_ICON_CALLBACK_MOUSE_RBDOWN,
    TRAY_ICON_CALLBACK_MOUSE_RBDBC,
    TRAY_ICON_CALLBACK_MOUSE_MBUP,
    TRAY_ICON_CALLBACK_MOUSE_MBDOWN,
    TRAY_ICON_CALLBACK_MOUSE_MBDBC,
    TRAY_ICON_CALLBACK_NOTIFICATION_CLICK,
    TRAY_ICON_CALLBACK_NOTIFICATION_TIMEOUT,
} TrayIconCallbackTypeIndex;

typedef struct {
    PyObject_HEAD
    UINT id;
    PyObject *tip;
    BOOL hidden;
    IconHandleObject *icon_handle;

    uint16_t callback_flags;
    PyObject *callbacks[12];
} TrayIconObject;

BOOL update_tray_icon(TrayIconObject* tray_icon, DWORD message, UINT flags, void *reserved);

// Caller must hold `tray_window_cs` critical section
#define PWT_ADD_ICON_TO_TRAY(tray_icon) \
    (update_tray_icon(tray_icon, NIM_ADD, NIF_MESSAGE|NIF_TIP|NIF_ICON|NIF_STATE, NULL))
// Caller must hold `tray_window_cs` critical section
#define PWT_DELETE_ICON_FROM_TRAY(tray_icon) \
    (update_tray_icon(tray_icon, NIM_DELETE, 0, NULL))

// TrayIcon end

// Menu start

typedef struct {
    union { // header
        PyTypeObject type;

        // Although Menu itself is not a heap type,
        // the subclasses of Menu will still be heap types
        // So we need to reserve space for PyHeapTypeObject
        PyHeapTypeObject heap_type;
    };
    PyObject *items_list;
    HMENU handle;
    HWND parent_window;
    HANDLE popup_event;
} MenuTypeObject;

BOOL menu_subtype_check(PyObject *arg);

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
    ULONG_PTR update_counter;
    PyObject *string;
    BOOL enabled;
    PyObject *callback;
    BOOL checked;
    BOOL radio;
    MenuTypeObject*sub;
} MenuItemObject;

BOOL update_menu_item(HMENU menu, UINT pos, MenuItemObject *menu_item, BOOL insert);
BOOL update_all_items_in_menu(MenuTypeObject *cls, BOOL insert);

// MenuItem end

// _test_api start

PyObject *create_test_api();
PyObject *_idm_get_internal_dict(IDManager *idm);

// _test_api end

// globals start

typedef struct {
    ATOM window_class_atom;
    HANDLE tray_loop_ready_event;
    IDManager *tray_icon_idm;
    IDManager *menu_item_idm;
    IDManager *active_menus_idm; // id:hwnd value:menu
    
    CRITICAL_SECTION tray_window_cs;
    HWND tray_window;
    LONG tray_loop_started;

    PyTypeObject *IconHandleType;
    PyTypeObject *TrayIconType;
    PyTypeObject *MenuItemType;
    MenuTypeObject *MenuType;

} PWTGlobals;

extern PWTGlobals pwt_globals;

#define PWT_ENTER_TRAY_WINDOW_CS() (EnterCriticalSection(&(pwt_globals.tray_window_cs)))
#define PWT_LEAVE_TRAY_WINDOW_CS() (LeaveCriticalSection(&(pwt_globals.tray_window_cs)))

// Caller must hold `tray_window_cs` critical section
#define PWT_TRAY_WINDOW_AVAILABLE() (!(!(pwt_globals.tray_window)))

PyTypeObject *create_icon_handle_type(PyObject *module);
PyTypeObject *create_tray_icon_type(PyObject *module);
PyTypeObject *create_menu_item_type(PyObject *module);
MenuTypeObject *create_menu_type(PyObject *module);

// globals end

#endif // PYWINTRAY_H
