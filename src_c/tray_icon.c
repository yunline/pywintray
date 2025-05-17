/*
This file implements the pywintray.TrayIcon class
*/

#include "pywintray.h"

IDManager *tray_icon_idm = NULL;

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

BOOL
show_icon(TrayIconObject* tray_icon) {
    return notify(tray_icon, NIM_ADD, NIF_MESSAGE|NIF_TIP|NIF_ICON);
}

static BOOL
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

    self->callback_flags = 0;
    for(int i=0;i<sizeof(self->callbacks)/sizeof(self->callbacks[0]);i++) {
        self->callbacks[i] = NULL;
    }

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

    return 0;
}

static PyObject*
tray_icon_show(TrayIconObject* self, PyObject* args) {
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
tray_icon_update_icon(TrayIconObject *self, PyObject *arg) {
    if(!PyObject_IsInstance(arg, (PyObject *)&IconHandleType)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be an IconHandle");
        return NULL;
    }

    IconHandleObject* old_icon = self->icon_handle;
    self->icon_handle = (IconHandleObject *)arg;

    if(!self->hidden && MAINLOOP_RUNNING()) {
        if (!notify(self, NIM_MODIFY, NIF_ICON)) {
            self->icon_handle = old_icon;
            return NULL;
        }
    }

    Py_DECREF(old_icon);
    Py_INCREF(arg);

    Py_RETURN_NONE;
}

static PyObject*
tray_icon_register_callback(TrayIconObject *self, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"callback_type", "callback", NULL};

    PyObject *callback_type_str_obj;
    PyObject *callback_object = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", kwlist, &callback_type_str_obj, &callback_object)) {
        return NULL;
    }

    TrayIconCallbackTypeIndex callback_type;

    if (!PyUnicode_Check(callback_type_str_obj)) {
        PyErr_SetString(PyExc_TypeError, "Argument 'callback_type' must be a str");
        return NULL;
    }

    if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_move")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_MOVE;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_left_button_down")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_LBDOWN;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_left_button_up")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_LBUP;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_left_double_click")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_LBDBC;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_right_button_down")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_RBDOWN;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_right_button_up")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_RBUP;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_right_double_click")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_RBDBC;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_mid_button_down")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_MBDOWN;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_mid_button_up")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_MBUP;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "mouse_mid_double_click")) {
        callback_type = TRAY_ICON_CALLBACK_MOUSE_MBDBC;
    }
    else {
        PyErr_SetString(
            PyExc_ValueError, "Value of 'callback_type' must in "
            "['mouse_move', "
            "'mouse_left_button_down', 'mouse_left_button_up', 'mouse_left_double_click',"
            "'mouse_right_button_down', 'mouse_right_button_up', 'mouse_right_double_click',"
            "'mouse_mid_button_down', 'mouse_mid_button_up', 'mouse_mid_double_click',"
            "]"
        );
        return NULL;
    }

    if (!callback_object) {
        // if user doesn't pass 'callback' parameter
        // return a decorator
        PyObject *l = Py_BuildValue("{s:O,s:O}", "s", self, "t", callback_type_str_obj);
        PyObject *g = Py_BuildValue("{}");
        PyRun_String("d=(lambda s,t:lambda c:(c,s.register_callback(t,c))[0])(s,t)", Py_file_input, g, l);
        PyObject *result =  PyDict_GetItemString(l, "d");
        Py_XINCREF(result);
        Py_DECREF(g);
        Py_DECREF(l);
        return result;
    }

    if(!Py_IsNone(callback_object) && !PyCallable_Check(callback_object)) {
        PyErr_SetString(PyExc_TypeError, "Callback should be callable");
        return NULL;
    }

    if (Py_IsNone(callback_object)) {
        self->callback_flags &= ~(1<<callback_type);
        Py_DECREF(self->callbacks[callback_type]);
        self->callbacks[callback_type] = NULL;
    }
    else {
        self->callback_flags |= (1<<callback_type);
        Py_INCREF(callback_object);
        self->callbacks[callback_type] = callback_object;
    }

    Py_RETURN_NONE;
}


static PyMethodDef tray_icon_methods[] = {
    {"show", (PyCFunction)tray_icon_show, METH_NOARGS, NULL},
    {"hide", (PyCFunction)tray_icon_hide, METH_NOARGS, NULL},
    {"update_icon", (PyCFunction)tray_icon_update_icon, METH_O, NULL},
    {"register_callback", (PyCFunction)tray_icon_register_callback, METH_VARARGS|METH_KEYWORDS, NULL},
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

    if (!self->hidden && MAINLOOP_RUNNING()) {
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
    if(self->hidden) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
tray_icon_get__internal_id(TrayIconObject *self, void *closure) {
    return PyLong_FromUnsignedLong(self->id);
}

static PyGetSetDef tray_icon_getset[] = {
    {"tip", (getter)tray_icon_get_tip, (setter)tray_icon_set_tip, NULL, NULL},
    {"hidden", (getter)tray_icon_get_hidden, (setter)NULL, NULL, NULL},
    {"_internal_id", (getter)tray_icon_get__internal_id, (setter)NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static void
tray_icon_dealloc(TrayIconObject *self)
{
    if(!self->hidden && MAINLOOP_RUNNING() && self->id) {
        if(!hide_icon(self)) {
            PyErr_Print();
        }
        self->hidden=TRUE;
    }

    if (self->id) {
        if(!idm_delete_id(tray_icon_idm, self->id)) {
            PyErr_Print();
        }
        self->id = 0;
    }
    Py_XDECREF(self->tip);
    Py_XDECREF(self->icon_handle);

    for(int i=0;i<sizeof(self->callbacks)/sizeof(self->callbacks[0]);i++) {
        Py_XDECREF(self->callbacks[i]);
    }

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
