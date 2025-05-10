/*
This file implements the pywintray.TrayIcon class
*/

#include "pywintray.h"

IDManager *tray_icon_idm = NULL;

#define CHECK_TRAY_ICON_VALID(tray_icon, retv) { \
    if (!((tray_icon)->valid)) { \
        PyErr_SetString(PyExc_RuntimeError, "tray icon has been destroyed");\
        return (retv);\
    } \
}

static BOOL
notify(TrayIconObject* tray_icon, DWORD message, UINT flags) {
    NOTIFYICONDATAW notify_data;
    notify_data.cbSize = sizeof(NOTIFYICONDATAW);
    notify_data.hWnd = message_window;
    notify_data.uID = tray_icon->id;
    notify_data.hIcon = NULL;
    notify_data.dwState = NIS_SHAREDICON;
    notify_data.uFlags = flags;
    if(flags&NIF_MESSAGE) {
        notify_data.uCallbackMessage = PYWINTRAY_MESSAGE;
    }
    if(flags&NIF_TIP) {
        if(-1==PyUnicode_AsWideChar(tray_icon->tip, notify_data.szTip, sizeof(notify_data.szTip))) {
            return FALSE;
        }
    }
    if(flags&NIF_ICON){
        if (tray_icon->icon_handle){
            notify_data.hIcon = tray_icon->icon_handle->icon_handle;
        }
    }

    if(!Shell_NotifyIcon(message, &notify_data)) {
        RAISE_LAST_ERROR();
        return FALSE;
    }

    return TRUE;
}

inline BOOL
show_icon(TrayIconObject* tray_icon) {
    return notify(tray_icon, NIM_ADD, NIF_MESSAGE|NIF_TIP|NIF_ICON);
}

static inline BOOL
hide_icon(TrayIconObject* tray_icon) {
    return notify(tray_icon, NIM_DELETE, 0);
}

static int
tray_icon_init(TrayIconObject *self, PyObject *args, PyObject* kwargs)
{
    static char *kwlist[] = {"icon_handle", "tip", "hidden", NULL};

    PyObject *icon_handle = NULL;
    PyObject *tip = NULL;

    self->id = idm_allocate_id(tray_icon_idm, self);
    if(!self->id) {
        return -1;
    }

    self->tip = NULL;
    self->hidden = FALSE;
    self->icon_handle = NULL;

    self->valid = FALSE;

    self->mouse_move_callback = NULL;
    self->mouse_button_down_callback=NULL;
    self->mouse_button_up_callback=NULL;
    self->mouse_double_click_callback=NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|Up", kwlist, 
        &icon_handle,
        &tip, 
        &(self->hidden)
    )) {
        return -1;
    }

    if(!PyObject_IsInstance(icon_handle, (PyObject *)&IconHandleType)) {
        PyErr_SetString(PyExc_TypeError, "'icon_handle' must be an IconHandle");
        return -1;
    }

    Py_INCREF(icon_handle);
    self->icon_handle = (IconHandleObject *)icon_handle;

    if(tip==NULL) {
        tip = Py_BuildValue("s", "pywintray");
    }
    else {
        Py_INCREF(tip);
    }
    self->tip = tip;


    if(!self->hidden && MAINLOOP_RUNNING()) {
        if(!show_icon(self)) {
            return -1;
        }
    }

    self->valid = TRUE;

    return 0;
}

static PyObject*
tray_icon_show(TrayIconObject* self, PyObject* args) {
    CHECK_TRAY_ICON_VALID(self, NULL);

    if (!self->hidden) {
        Py_RETURN_NONE;
    }

    if (!MAINLOOP_RUNNING()) {
        self->hidden = FALSE;
        Py_RETURN_NONE;
    }

    if(!show_icon(self)) {
        return NULL;
    }

    self->hidden = FALSE;

    Py_RETURN_NONE;
}

static PyObject*
tray_icon_hide(TrayIconObject* self, PyObject* args) {
    CHECK_TRAY_ICON_VALID(self, NULL);

    if (self->hidden) {
        Py_RETURN_NONE;
    }

    if (!MAINLOOP_RUNNING()) {
        self->hidden = TRUE;
        Py_RETURN_NONE;
    }

    if(!hide_icon(self)) {
        return NULL;
    }

    self->hidden = TRUE;

    Py_RETURN_NONE;
}

static PyObject*
tray_icon_destroy(TrayIconObject* self, PyObject* args) {
    if (self->id) {
        if(!idm_delete_id(tray_icon_idm, self->id)) {
            return NULL;
        }
        self->id = 0;
    }

    if (!(self->valid)) {
        Py_RETURN_NONE;
    }

    self->valid=FALSE;

    if(!self->hidden && MAINLOOP_RUNNING()) {
        if(!hide_icon(self)) {
            return NULL;
        }
        self->hidden=TRUE;
    }

    Py_RETURN_NONE;
}

static PyObject*
tray_icon_update_icon(TrayIconObject *self, PyObject *args, PyObject* kwargs) {
    CHECK_TRAY_ICON_VALID(self, NULL);

    static char *kwlist[] = {"icon_handle", NULL};

    PyObject *new_icon = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &new_icon)) {
        return NULL;
    }

    if(!PyObject_IsInstance(new_icon, (PyObject *)&IconHandleType)) {
        PyErr_SetString(PyExc_TypeError, "'icon_handle' must be an IconHandle");
        return NULL;
    }

    IconHandleObject* old_icon = self->icon_handle;
    self->icon_handle = (IconHandleObject *)new_icon;

    if(!self->hidden && MAINLOOP_RUNNING()) {
        if (!notify(self, NIM_MODIFY, NIF_ICON)) {
            self->icon_handle = old_icon;
            return NULL;
        }
    }

    Py_DECREF(old_icon);
    Py_INCREF(new_icon);

    Py_RETURN_NONE;
}

static PyMethodDef tray_icon_methods[] = {
    {"show", (PyCFunction)tray_icon_show, METH_NOARGS, NULL},
    {"hide", (PyCFunction)tray_icon_hide, METH_NOARGS, NULL},
    {"destroy", (PyCFunction)tray_icon_destroy, METH_NOARGS, NULL},
    {"update_icon", (PyCFunction)tray_icon_update_icon, METH_VARARGS|METH_KEYWORDS, NULL},
    {NULL, NULL, 0, NULL}
};


static PyObject *
tray_icon_get_tip(TrayIconObject *self, void *closure) {
    CHECK_TRAY_ICON_VALID(self, NULL);

    Py_INCREF(self->tip);
    return self->tip;
}

static int
tray_icon_set_tip(TrayIconObject *self, PyObject *value, void *closure) {
    CHECK_TRAY_ICON_VALID(self, -1);

    if (!PyUnicode_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'tip' must be a string");
        return -1;
    }

    PyObject *old_value = self->tip;
    self->tip = value;

    if (MAINLOOP_RUNNING()) {
        if (!notify(self, NIM_MODIFY, NIF_TIP)) {
            return -1;
        }
    }

    Py_INCREF(value);
    Py_DECREF(old_value);

    return 0;
}

static PyObject *
tray_icon_get_hidden(TrayIconObject *self, void *closure) {
    CHECK_TRAY_ICON_VALID(self, NULL);

    if(self->hidden) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
tray_icon_get_callback_generic(TrayIconObject *self, void *callback_addr_offset) {
    CHECK_TRAY_ICON_VALID(self, NULL);

    PyObject **callback_addr = (PyObject **)((intptr_t)self+(intptr_t)callback_addr_offset);

    if (*callback_addr) {
        Py_INCREF(*callback_addr);
        return *callback_addr;
    }
    Py_RETURN_NONE;
}

static int
tray_icon_set_callback_generic(TrayIconObject *self, PyObject *value, void *callback_addr_offset) {
    CHECK_TRAY_ICON_VALID(self, -1);

    PyObject **callback_addr = (PyObject **)((intptr_t)self+(intptr_t)callback_addr_offset);

    if (Py_IsNone(value)) {
        Py_XDECREF(*callback_addr);
        *callback_addr = NULL;
        return 0;
    }
    if (!PyCallable_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "callback must be a callable or None");
        return -1;
    }

    Py_XDECREF(*callback_addr);
    *callback_addr = value;
    Py_INCREF(*callback_addr);

    return 0;
}

#define TRAY_ICON_CALLBACK_GET_SET(cb_name) \
{ \
    "on_"#cb_name, \
    (getter)tray_icon_get_callback_generic, \
    (setter)tray_icon_set_callback_generic, \
    NULL, \
    (void *)offsetof(TrayIconObject, ##cb_name##_callback)\
}

static PyGetSetDef tray_icon_getset[] = {
    {"tip", (getter)tray_icon_get_tip, (setter)tray_icon_set_tip, NULL, NULL},
    {"hidden", (getter)tray_icon_get_hidden, (setter)NULL, NULL, NULL},
    TRAY_ICON_CALLBACK_GET_SET(mouse_move),
    TRAY_ICON_CALLBACK_GET_SET(mouse_button_down),
    TRAY_ICON_CALLBACK_GET_SET(mouse_button_up),
    TRAY_ICON_CALLBACK_GET_SET(mouse_double_click),
    {NULL, NULL, NULL, NULL, NULL}
};

#undef TRAY_ICON_CALLBACK_GET_SET

static void
tray_icon_dealloc(TrayIconObject *self)
{
    tray_icon_destroy(self, NULL);
    Py_XDECREF(self->tip);
    Py_XDECREF(self->icon_handle);
    Py_XDECREF(self->mouse_move_callback);
    Py_XDECREF(self->mouse_button_down_callback);
    Py_XDECREF(self->mouse_button_up_callback);
    Py_XDECREF(self->mouse_double_click_callback);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PyTypeObject TrayIconType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pywintray.TrayIcon",
    .tp_doc = NULL,
    .tp_basicsize = sizeof(TrayIconObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)tray_icon_init,
    .tp_dealloc = (destructor)tray_icon_dealloc,
    .tp_methods = tray_icon_methods,
    .tp_getset = tray_icon_getset,
};
