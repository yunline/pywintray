/*
This file implements the pywintray.TrayIcon class
*/

#include "pywintray.h"
#include <shellapi.h>

static UINT tray_icon_id_counter = 1;

static BOOL
build_notify_data(NOTIFYICONDATAW *notify_data, TrayIconObject* tray_icon, UINT flags) {
    notify_data->cbSize = sizeof(NOTIFYICONDATAW);
    notify_data->hWnd = message_window;
    notify_data->uID = tray_icon->id;
    notify_data->hIcon = NULL;
    notify_data->dwState = NIS_SHAREDICON;
    notify_data->uFlags = flags;
    if(flags&NIF_MESSAGE) {
        notify_data->uCallbackMessage = CALLBACK_MESSAGE;
    }
    if(flags&NIF_TIP) {
        if(-1==PyUnicode_AsWideChar(tray_icon->tip, notify_data->szTip, sizeof(notify_data->szTip))) {
            return FALSE;
        }
    }
    if(flags&NIF_ICON){
        if (tray_icon->icon_handle){
            notify_data->hIcon = tray_icon->icon_handle;
        }
    }
    return TRUE;
}

static BOOL
show_icon(TrayIconObject* tray_icon) {
    NOTIFYICONDATAW notify_data;
    if(!build_notify_data(&notify_data, tray_icon, NIF_MESSAGE|NIF_TIP|NIF_ICON)) {
        return FALSE;
    }

    if(!Shell_NotifyIcon(NIM_ADD, &notify_data)) {
        RAISE_LAST_ERROR();
        return FALSE;
    }

    return TRUE;
}

static BOOL
hide_icon(TrayIconObject* tray_icon) {
    NOTIFYICONDATAW notify_data;
    if(!build_notify_data(&notify_data, tray_icon, 0)) {
        return FALSE;
    }

    if(!Shell_NotifyIcon(NIM_DELETE, &notify_data)) {
        RAISE_LAST_ERROR();
        return FALSE;
    }

    return TRUE;
}

static int
tray_icon_init(TrayIconObject *self, PyObject *args, PyObject* kwargs)
{
    static char *kwlist[] = {"icon_path","icon_handle","tip","hidden","load_icon_large","load_icon_index", NULL};

    PyObject *icon_handle_obj = Py_None;

    PyObject *icon_filename_obj = Py_None;
    wchar_t filename[MAX_PATH];
    BOOL large = TRUE;
    int index = 0;
    UINT result;

    self->tip = NULL;
    self->hidden = FALSE;
    self->id = tray_icon_id_counter;
    self->icon_handle = NULL;
    self->icon_need_free = TRUE;
    
    tray_icon_id_counter++;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OOUppi", kwlist, 
        &icon_filename_obj,
        &icon_handle_obj,
        &(self->tip), 
        &(self->hidden),
        &large,
        &index
    )) {
        return -1;
    }

    if(!Py_IsNone(icon_handle_obj)) {
        if(!Py_IsNone(icon_filename_obj)) {
            PyErr_SetString(PyExc_TypeError, "You can not set 'icon_handle' when 'icon_path' is set");
            return -1;
        }
        if(!PyLong_Check(icon_handle_obj)) {
            PyErr_SetString(PyExc_TypeError, "'icon_handle' must be an int");
            return -1;
        }

        self->icon_handle = (HICON)PyLong_AsVoidPtr(icon_handle_obj);
        if(self->icon_handle==NULL) {
            if(PyErr_Occurred()) {
                return -1;
            }
            PyErr_SetString(PyExc_ValueError, "'icon_handle' is invalid (NULL)");
            return -1;
        }
        self->icon_need_free = FALSE;
    }
    else {
        if(Py_IsNone(icon_filename_obj)) {
            PyErr_SetString(PyExc_TypeError, "You need set one of 'icon_path' or 'icon_handle'");
            return -1;
        }
        if(!PyUnicode_Check(icon_filename_obj)) {
            PyErr_SetString(PyExc_TypeError, "'icon_path' must be an str");
            return -1;
        }
    
        
        if(-1==PyUnicode_AsWideChar(icon_filename_obj, filename, sizeof(filename))) {
            return -1;
        }
    
        Py_BEGIN_ALLOW_THREADS;
        if (large){
            result = ExtractIconEx(filename, index, &(self->icon_handle), NULL, 1);
        }
        else {
            result = ExtractIconEx(filename, index, NULL, &(self->icon_handle), 1);
        }
        Py_END_ALLOW_THREADS;
    
        if(result==UINT_MAX) {
            RAISE_LAST_ERROR();
            return -1;
        }
        if(self->icon_handle==NULL) {
            PyErr_SetString(PyExc_OSError, "Unable to load icon");
            return -1;
        }
    }

    if(self->tip==NULL) {
        self->tip = Py_BuildValue("s", "pywintray");
    }
    else {
        Py_INCREF(self->tip);
    }

    if(!self->hidden) {
        if(!show_icon(self)) {
            Py_DECREF(self->tip);
            return -1;
        }
    }
    
    return 0;
}

static PyObject*
tray_icon_show(TrayIconObject* self, PyObject* args) {
    if (!self->hidden) {
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
    if (self->hidden) {
        Py_RETURN_NONE;
    }

    if(!hide_icon(self)) {
        return NULL;
    }

    self->hidden = FALSE;

    Py_RETURN_NONE;
}

static PyMethodDef tray_icon_methods[] = {
    {"show", (PyCFunction)tray_icon_show, METH_NOARGS, NULL},
    {"hide", (PyCFunction)tray_icon_hide, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL}
};


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

    PyObject *old_value = self->tip;
    self->tip = value;

    NOTIFYICONDATAW notify_data;
    if(!build_notify_data(&notify_data, self, NIF_TIP)) {
        return -1;
    }

    if(!Shell_NotifyIcon(NIM_MODIFY, &notify_data)) {
        RAISE_LAST_ERROR();
        return -1;
    }

    Py_INCREF(value);
    Py_DECREF(old_value);

    return 0;
}

static PyObject *
tray_icon_get_hidden(TrayIconObject *self, void *closure) {
    if(self->hidden) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyGetSetDef tray_getseters[] = {
    {"tip", (getter)tray_icon_get_tip, (setter)tray_icon_set_tip, NULL, NULL},
    {"hidden", (getter)tray_icon_get_hidden, (setter)NULL, NULL, NULL},
    {NULL, NULL, 0, NULL, NULL}
};

static void
tray_icon_dealloc(TrayIconObject *self)
{
    Py_XDECREF(self->tip);
    DestroyIcon(self->icon_handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
tray_icon_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    TrayIconObject *self = (TrayIconObject *)type->tp_alloc(type, 0);
    return (PyObject *)self;
}

PyTypeObject TrayIconType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pywintray.TrayIcon",
    .tp_doc = NULL,
    .tp_basicsize = sizeof(TrayIconObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = tray_icon_new,
    .tp_init = (initproc)tray_icon_init,
    .tp_dealloc = (destructor)tray_icon_dealloc,
    .tp_methods = tray_icon_methods,
    .tp_getset = tray_getseters,
};
