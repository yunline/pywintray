/*
This file implements the pywintray.TrayIcon class
*/

#include "pywintray.h"

typedef struct {
    PyObject* msg_str_obj;
    PyObject* title_str_obj;
    HICON icon;
    DWORD flags;
} ToastData;

// Caller must hold `tray_window_cs` critical section
BOOL
update_tray_icon(
    TrayIconObject* tray_icon, 
    DWORD message, UINT flags, 
    ToastData *toast_data
) {
    NOTIFYICONDATAW notify_data;
    notify_data.cbSize = sizeof(notify_data);
    notify_data.hWnd = pwt_globals.tray_window;
    notify_data.uID = tray_icon->id;
    notify_data.hIcon = NULL;
    notify_data.uFlags = flags;
    if(flags&NIF_MESSAGE) {
        notify_data.uCallbackMessage = PYWINTRAY_TRAY_MESSAGE;
    }
    if(flags&NIF_TIP) {
        const Py_ssize_t buf_size = sizeof(notify_data.szTip)/sizeof(WCHAR);
        if(-1==PyUnicode_AsWideChar(tray_icon->tip, notify_data.szTip, buf_size)) {
            return FALSE;
        }
    }
    if(flags&NIF_ICON){
        if (tray_icon->icon_handle){
            notify_data.hIcon = tray_icon->icon_handle->icon_handle;
        }
    }
    if (flags&NIF_STATE) {
        notify_data.dwStateMask = NIS_HIDDEN;
        if (tray_icon->hidden) {
            notify_data.dwState = NIS_HIDDEN;
        }
        else {
            notify_data.dwState = 0;
        }
    }
    if (flags&NIF_INFO) {
        PyObject *tmp_str = NULL;
        Py_ssize_t result;
        if (PyUnicode_GetLength(toast_data->msg_str_obj)==0) {
            // for an empty message string, replace it with a single " "
            tmp_str = PyUnicode_FromString(" ");
            toast_data->msg_str_obj = tmp_str;
        } 
        else if (PyErr_Occurred()) {
            return FALSE;
        }

        const Py_ssize_t msg_buf_size = sizeof(notify_data.szInfo)/sizeof(WCHAR);
        result = PyUnicode_AsWideChar(toast_data->msg_str_obj, notify_data.szInfo, msg_buf_size);
        Py_XDECREF(tmp_str);
        if(result < 0) {
            return FALSE;
        }

        const Py_ssize_t title_buf_size = sizeof(notify_data.szInfoTitle)/sizeof(WCHAR);
        result = PyUnicode_AsWideChar(toast_data->title_str_obj, notify_data.szInfoTitle, title_buf_size);
        if(result < 0) {
            return FALSE;
        }

        notify_data.dwInfoFlags = toast_data->flags;
        notify_data.hBalloonIcon = toast_data->icon;
    }

    if(!Shell_NotifyIcon(message, &notify_data)) {
        RAISE_LAST_ERROR();
        return FALSE;
    }

    return TRUE;
}

static PyObject *
tray_icon_new(PyTypeObject *cls, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"icon_handle", "tip", "hidden", NULL};

    PyObject *icon_handle = NULL;
    PyObject *tip = NULL;

    // new
    TrayIconObject *self = (TrayIconObject *)PyType_GenericNew(cls, args, kwargs);

    // init struct
    self->id = 0;
    self->tip = NULL;
    self->hidden = FALSE;
    self->icon_handle = NULL;
    self->callback_flags = 0;
    for(int i=0;i<sizeof(self->callbacks)/sizeof(self->callbacks[0]);i++) {
        self->callbacks[i] = NULL;
    }

    // parse args
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|Up", kwlist, 
        &icon_handle,
        &tip, 
        &(self->hidden)
    )) {
        goto error_clean_up;
    }

    if(!PyObject_IsInstance(icon_handle, (PyObject *)(pwt_globals.IconHandleType))) {
        PyErr_SetString(PyExc_TypeError, "'icon_handle' must be an IconHandle");
        goto error_clean_up;
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

    // allocate id
    self->id = idm_allocate_id(pwt_globals.tray_icon_idm, self);
    if(!self->id) {
        goto error_clean_up;
    }

    // update (if possible)
    BOOL result = TRUE;
    PWT_ENTER_TRAY_WINDOW_CS();
    if(PWT_TRAY_WINDOW_AVAILABLE()) {
        result = PWT_ADD_ICON_TO_TRAY(self);
    }
    PWT_LEAVE_TRAY_WINDOW_CS();

    if (!result) {
        goto error_clean_up;
    }

    return (PyObject *)self;

error_clean_up:
    Py_DECREF(self);
    return NULL;
}

static int
tray_icon_set_hidden(TrayIconObject *self, PyObject *value, void *closure);

static PyObject*
tray_icon_show(TrayIconObject* self, PyObject* args) {
    if (tray_icon_set_hidden(self, Py_False, NULL)<0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject*
tray_icon_hide(TrayIconObject* self, PyObject* args) {
    if (tray_icon_set_hidden(self, Py_True, NULL)<0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject*
tray_icon_register_callback(TrayIconObject *self, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"callback_type", "callback", NULL};

    PyObject *callback_type_str_obj;
    PyObject *callback_object = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "U|O", kwlist, &callback_type_str_obj, &callback_object)) {
        return NULL;
    }

    TrayIconCallbackTypeIndex callback_type;

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
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "notification_click")) {
        callback_type = TRAY_ICON_CALLBACK_NOTIFICATION_CLICK;
    }
    else if (PyUnicode_EqualToUTF8(callback_type_str_obj, "notification_timeout")) {
        callback_type = TRAY_ICON_CALLBACK_NOTIFICATION_TIMEOUT;
    }
    else {
        PyErr_SetString(
            PyExc_ValueError, "Value of 'callback_type' must in "
            "['mouse_move',"
            " 'mouse_left_button_down', 'mouse_left_button_up', 'mouse_left_double_click',"
            " 'mouse_right_button_down', 'mouse_right_button_up', 'mouse_right_double_click',"
            " 'mouse_mid_button_down', 'mouse_mid_button_up', 'mouse_mid_double_click',"
            " 'notification_click', 'notification_timeout',"
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


static PyObject*
tray_icon_notify(TrayIconObject *self, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"title", "message", "no_sound", "icon", NULL};

    ToastData toast_data;
    toast_data.flags = 0;

    BOOL no_sound = FALSE;
    PyObject *icon_obj = NULL;
    HICON copied_icon = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "UU|pO", kwlist, 
        &(toast_data.title_str_obj),
        &(toast_data.msg_str_obj), 
        &no_sound,
        &icon_obj
    )) {
        return NULL;
    }

    if (!icon_obj || Py_IsNone(icon_obj)) {
        toast_data.flags |= NIIF_USER;
        toast_data.icon = NULL;
    }
    else {
        int is_icon_handle = PyObject_IsInstance(icon_obj, (PyObject *)(pwt_globals.IconHandleType));
        if (is_icon_handle<0) {
            return NULL;
        }
        if (!is_icon_handle) {
            PyErr_SetString(PyExc_TypeError, "'icon' should be IconHandle or None");
            return NULL;
        }
        toast_data.flags |= NIIF_USER;
        toast_data.flags |= NIIF_LARGE_ICON;
        // convert as large icon
        copied_icon = CopyImage(
            ((IconHandleObject *)icon_obj)->icon_handle, 
            IMAGE_ICON, 
            GetSystemMetrics(SM_CXICON),
            GetSystemMetrics(SM_CYICON),
            LR_COPYFROMRESOURCE
        );
        if (!copied_icon) {
            RAISE_LAST_ERROR();
            return NULL;
        }
        toast_data.icon = copied_icon;
    }

    if (no_sound) {
        toast_data.flags |= NIIF_NOSOUND;
    }

    BOOL result = TRUE;
    PWT_ENTER_TRAY_WINDOW_CS();
    if (PWT_TRAY_WINDOW_AVAILABLE()) {
        result = update_tray_icon(self, NIM_MODIFY, NIF_INFO, &toast_data);
    }
    PWT_LEAVE_TRAY_WINDOW_CS();

    if (copied_icon) {
        DestroyIcon(copied_icon);
    }

    if (!result) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyMethodDef tray_icon_methods[] = {
    {"show", (PyCFunction)tray_icon_show, METH_NOARGS, NULL},
    {"hide", (PyCFunction)tray_icon_hide, METH_NOARGS, NULL},
    {"register_callback", (PyCFunction)tray_icon_register_callback, METH_VARARGS|METH_KEYWORDS, NULL},
    {"notify", (PyCFunction)tray_icon_notify, METH_VARARGS|METH_KEYWORDS, NULL},
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

    BOOL result = TRUE;
    PWT_ENTER_TRAY_WINDOW_CS();
    if (PWT_TRAY_WINDOW_AVAILABLE()) {
        result = update_tray_icon(self, NIM_MODIFY, NIF_TIP, NULL);
    }
    PWT_LEAVE_TRAY_WINDOW_CS();

    if (!result) {
        self->tip = old_value;
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

static int
tray_icon_set_hidden(TrayIconObject *self, PyObject *value, void *closure) {
    int hidden = PyObject_IsTrue(value);
    if (hidden<0) {
        return -1;
    }

    BOOL old_value = self->hidden;
    self->hidden = hidden;
    
    BOOL result = TRUE;
    PWT_ENTER_TRAY_WINDOW_CS();
    if (PWT_TRAY_WINDOW_AVAILABLE()) {
        result = update_tray_icon(self, NIM_MODIFY, NIF_STATE, NULL);
    }
    PWT_LEAVE_TRAY_WINDOW_CS();

    if (!result) {
        self->hidden = old_value;
        return -1;
    }

    return 0;
}

static PyObject *
tray_icon_get_icon_handle(TrayIconObject *self, void *closure) {
    Py_INCREF(self->icon_handle);
    return (PyObject *)self->icon_handle;
}

static int
tray_icon_set_icon_handle(TrayIconObject *self, PyObject *value, void *closure) {
    if(!PyObject_IsInstance(value, (PyObject *)(pwt_globals.IconHandleType))) {
        PyErr_SetString(PyExc_TypeError, "icon_handle must be an IconHandle");
        return -1;
    }

    IconHandleObject* old_icon = self->icon_handle;
    self->icon_handle = (IconHandleObject *)value;

    BOOL result = TRUE;
    PWT_ENTER_TRAY_WINDOW_CS();
    if(PWT_TRAY_WINDOW_AVAILABLE()) {
        result = update_tray_icon(self, NIM_MODIFY, NIF_ICON, NULL);
    }
    PWT_LEAVE_TRAY_WINDOW_CS();

    if (!result) {
        self->icon_handle = old_icon;
        return -1;
    }

    Py_DECREF(old_icon);
    Py_INCREF(value);

    return 0;
}

static PyGetSetDef tray_icon_getset[] = {
    {"tip", (getter)tray_icon_get_tip, (setter)tray_icon_set_tip, NULL, NULL},
    {"hidden", (getter)tray_icon_get_hidden, (setter)tray_icon_set_hidden, NULL, NULL},
    {"icon_handle", (getter)tray_icon_get_icon_handle, (setter)tray_icon_set_icon_handle, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static void
tray_icon_dealloc(TrayIconObject *self) {
    if(self->id) {
        PWT_ENTER_TRAY_WINDOW_CS();
        if (PWT_TRAY_WINDOW_AVAILABLE() && (!PWT_DELETE_ICON_FROM_TRAY(self))) {
            PyErr_Print();
        }
        PWT_LEAVE_TRAY_WINDOW_CS();

        if(!idm_delete_id(pwt_globals.tray_icon_idm, self->id)) {
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

PyTypeObject *
create_tray_icon_type(PyObject *module) {
    static PyType_Spec spec;

    PyType_Slot slots[] = {
        {Py_tp_methods, tray_icon_methods},
        {Py_tp_getset, tray_icon_getset},
        {Py_tp_new, tray_icon_new},
        {Py_tp_dealloc, tray_icon_dealloc},
        {0, NULL}
    };

    spec.name = "pywintray.TrayIcon";
    spec.basicsize = sizeof(TrayIconObject);
    spec.itemsize = 0;
    spec.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE;
    spec.slots = slots;

    return (PyTypeObject *)PyType_FromModuleAndSpec(module, &spec, NULL);
}
