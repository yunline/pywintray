/*
This file implements the pywintray.MenuItem class
*/

#include "pywintray.h"

BOOL
check_menu_item_type_valid(MenuItemObject *menu_item) {
    if (
        (menu_item->type!=MENU_ITEM_TYPE_SEPARATOR) &&
        (menu_item->type!=MENU_ITEM_TYPE_STRING) &&
        (menu_item->type!=MENU_ITEM_TYPE_CHECK) &&
        (menu_item->type!=MENU_ITEM_TYPE_SUBMENU)
    ) {
        return FALSE;
    }
    return TRUE;
}

#define CHECK_MENU_ITEM_TYPE_VALID(mi, retv) \
    if (!check_menu_item_type_valid((mi))) { \
        PyErr_SetString(PyExc_SystemError, "Invalid menu item type"); \
        return (retv); \
    }

static MenuItemObject *
new_menu_item() {
    PyTypeObject *cls = pwt_globals.MenuItemType;
    return (MenuItemObject *)(cls->tp_alloc(cls, 0));
}

static int
init_menu_item_generic(MenuItemObject *self) {
    self->id = idm_allocate_id(pwt_globals.menu_item_idm, self);
    if(!self->id) {
        return -1;
    }

    self->type = MENU_ITEM_TYPE_NULL;
    self->update_counter = 0;
    self->string = NULL;
    self->enabled = TRUE;
    self->callback = NULL;
    self->checked = FALSE;
    self->radio = FALSE;
    self->sub = NULL;

    return 0;
}

static PyObject *
menu_item_separator(PyObject *cls, PyObject *args) {
    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }
    if(init_menu_item_generic(self)<0) {
        Py_DECREF(self);
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_SEPARATOR;
    return (PyObject *)self;
}

static PyObject *
menu_item_string(PyObject *cls, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"label", "enabled", "callback", NULL};

    PyObject *string_obj = NULL;
    PyObject *callback_obj = Py_None;
    BOOL enabled = TRUE;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U|pO", kwlist, 
        &string_obj, &enabled, &callback_obj
    )) {
        return NULL;
    }

    if (!Py_IsNone(callback_obj) && !PyCallable_Check(callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback should be callable or None");
        return NULL;
    }

    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }
    if(init_menu_item_generic(self)<0) {
        Py_DECREF(self);
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_STRING;
    self->string = string_obj;
    Py_INCREF(string_obj);
    self->enabled = enabled;
    if (!Py_IsNone(callback_obj)) {
        self->callback = callback_obj;
        Py_INCREF(callback_obj);
    }

    return (PyObject *)self;
}

static PyObject *
menu_item_check(PyObject *cls, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"label", "radio", "checked", "enabled", "callback", NULL};

    PyObject *string_obj = NULL;
    PyObject *callback_obj = Py_None;

    BOOL checked = FALSE;
    BOOL radio = FALSE;
    BOOL enabled = TRUE;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U|pppO", kwlist,
        &string_obj,
        &radio,
        &checked,
        &enabled,
        &callback_obj
    )) {
        return NULL;
    }

    if (!Py_IsNone(callback_obj) && !PyCallable_Check(callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback should be callable or None");
        return NULL;
    }

    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }
    if(init_menu_item_generic(self)<0) {
        Py_DECREF(self);
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_CHECK;
    self->string = string_obj;
    Py_INCREF(string_obj);
    self->enabled = TRUE;
    self->checked = checked;
    self->radio = radio;
    if (!Py_IsNone(callback_obj)) {
        self->callback = callback_obj;
        Py_INCREF(callback_obj);
    }

    return (PyObject *)self;
}

static PyObject *menu_item_submenu_decorator(MenuItemObject* self, PyObject *arg);

static PyMethodDef submenu_decorator_method_def = {
    .ml_name = "_submenu_decorator",
    .ml_meth = (PyCFunction)menu_item_submenu_decorator,
    .ml_flags = METH_O,
    .ml_doc = NULL
};

static PyObject *
menu_item_sbumenu(PyObject *cls, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"label", "enabled", NULL};

    PyObject *string_obj = NULL;
    BOOL enabled = TRUE;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U|p", kwlist, &string_obj, &enabled)) {
        return NULL;
    }

    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }
    if(init_menu_item_generic(self)<0) {
        Py_DECREF(self);
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_SUBMENU;
    self->string = string_obj;
    Py_INCREF(string_obj);
    self->enabled = enabled;

    PyObject *decorator = PyCFunction_New(&submenu_decorator_method_def, (PyObject *)self);
    
    Py_DECREF(self);

    return decorator;
}

static PyObject *
menu_item_submenu_decorator(MenuItemObject* self, PyObject *arg) {
    if(self->type!=MENU_ITEM_TYPE_SUBMENU) {
        PyErr_SetString(PyExc_TypeError, "This method is for submenu only");
        return NULL;
    }

    if (self->sub) {
        PyErr_SetString(PyExc_RuntimeError, "You can only set the submenu once");
        return NULL;
    }

    if(!menu_subtype_check(arg)) {
        PyErr_SetString(
            PyExc_TypeError,
            "Error when calling submenu decorator: "
            "decorated object should be a subtype of Menu"
        );
        return NULL;
    }

    self->sub = (MenuTypeObject *)arg;
    
    Py_INCREF(arg);
    Py_INCREF(self);

    return (PyObject *)self;
}

static PyObject *
menu_item_register_callback(MenuItemObject* self, PyObject *arg) {
    if (
        (self->type!=MENU_ITEM_TYPE_STRING) &&
        (self->type!=MENU_ITEM_TYPE_CHECK)
    ) {
        PyErr_SetString(PyExc_TypeError, "This menu item doesn't support callback");
        return NULL;
    }

    if (Py_IsNone(arg)) {
        Py_XDECREF(self->callback);
        self->callback = NULL;
        Py_RETURN_NONE;
    }

    if(!PyCallable_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Callback should be callable or None");
        return NULL;
    }

    self->callback = arg;

    // store the object to self->callback, incref
    Py_INCREF(arg);

    // return the object, incref
    Py_INCREF(arg);

    return arg;
}

static PyMethodDef menu_item_metaclass_methods[] = {
    {"separator", (PyCFunction)menu_item_separator, METH_NOARGS, NULL},
    {"string", (PyCFunction)menu_item_string, METH_VARARGS|METH_KEYWORDS, NULL},
    {"check", (PyCFunction)menu_item_check, METH_VARARGS|METH_KEYWORDS, NULL},
    {"submenu", (PyCFunction)menu_item_sbumenu, METH_VARARGS|METH_KEYWORDS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyMethodDef menu_item_methods[] = {
    {"register_callback", (PyCFunction)menu_item_register_callback, METH_O, NULL},
    {NULL, NULL, 0, NULL}
};

static void
post_update_message(MenuItemObject *menu_item) {
    UINT hwnd_u32;
    Py_ssize_t pos = 0;

    idm_enter_critical_section(pwt_globals.active_menus_idm);
    while (idm_next(pwt_globals.active_menus_idm, &pos, &hwnd_u32, NULL)) {
        if (hwnd_u32==((UINT)-1) && PyErr_Occurred()) {
            PyErr_Print();
            continue;
        }
        HWND hwnd = (HWND)(intptr_t)hwnd_u32;
        PostMessage(hwnd, PYWINTRAY_MENU_UPDATE_MESSAGE, 0, (LPARAM)menu_item);
    }
    idm_leave_critical_section(pwt_globals.active_menus_idm);
}

static PyObject *
menu_item_get_sub(MenuItemObject *self, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_SUBMENU) {
        PyErr_SetString(PyExc_TypeError, "This property is for submenu only");
        return NULL;
    }
    if(!self->sub) {
        PyErr_SetString(PyExc_TypeError, "Submenu is not registered");
        return NULL;
    }
    Py_INCREF(self->sub);
    return (PyObject *)self->sub;
}

static PyObject *
menu_item_get_label(MenuItemObject *self, void *closure) {
    if(self->type==MENU_ITEM_TYPE_SEPARATOR) {
        PyErr_SetString(PyExc_TypeError, "Separator doesn't support label property");
        return NULL;
    }
    Py_INCREF(self->string);
    return self->string;
}

static int
menu_item_set_label(MenuItemObject *self, PyObject *value, void *closure) {
    if(self->type==MENU_ITEM_TYPE_SEPARATOR) {
        PyErr_SetString(PyExc_TypeError, "Separator doesn't support label property");
        return -1;
    }
    if(!PyUnicode_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Type of 'label' must be str");
        return -1;
    }
    Py_DECREF(self->string);
    self->string = value;
    Py_INCREF(self->string);
    self->update_counter++;
    post_update_message(self);
    return 0;
}

static PyObject *
menu_item_get_checked(MenuItemObject *self, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_TypeError, "This property is for check only");
        return NULL;
    }
    if (self->checked) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static int
menu_item_set_checked(MenuItemObject *self, PyObject *value, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_TypeError, "This property is for check only");
        return -1;
    }
    int result = PyObject_IsTrue(value);
    if(result<0) {
        return -1;
    }
    self->checked = result;
    self->update_counter++;
    post_update_message(self);
    return 0;
}

static PyObject *
menu_item_get_radio(MenuItemObject *self, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_TypeError, "This property is for check only");
        return NULL;
    }
    if (self->radio) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static int
menu_item_set_radio(MenuItemObject *self, PyObject *value, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_TypeError, "This property is for check only");
        return -1;
    }
    int result = PyObject_IsTrue(value);
    if(result<0) {
        return -1;
    }
    self->radio = result;
    self->update_counter++;
    post_update_message(self);
    return 0;
}

static PyObject *
menu_item_get_enabled(MenuItemObject *self, void *closure) {
    if(self->type==MENU_ITEM_TYPE_SEPARATOR) {
        PyErr_SetString(PyExc_TypeError, "Separator doesn't support this property");
        return NULL;
    }
    if (self->enabled) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static int
menu_item_set_enabled(MenuItemObject *self, PyObject *value, void *closure) {
    if(self->type==MENU_ITEM_TYPE_SEPARATOR) {
        PyErr_SetString(PyExc_TypeError, "Separator doesn't support this property");
        return -1;
    }
    int result = PyObject_IsTrue(value);
    if(result<0) {
        return -1;
    }
    self->enabled = result;
    self->update_counter++;
    post_update_message(self);
    return 0;
}

static PyObject *
menu_item_get_type(MenuItemObject *self, void *closure) {
    const char* type_string;
    switch (self->type) {
        case MENU_ITEM_TYPE_SEPARATOR:
            type_string="separator";
            break;
        case MENU_ITEM_TYPE_STRING:
            type_string="string";
            break;
        case MENU_ITEM_TYPE_CHECK:
            type_string="check";
            break;
        case MENU_ITEM_TYPE_SUBMENU:
            type_string="submenu";
            break;
        default:
            PyErr_Format(PyExc_SystemError, "Unknown menu item type %d", self->type);
            return NULL;
    }
    return PyUnicode_FromString(type_string);
}

static PyGetSetDef menu_item_getset[] = {
    {"sub", (getter)menu_item_get_sub, (setter)NULL, NULL, NULL},
    {"label", (getter)menu_item_get_label, (setter)menu_item_set_label, NULL, NULL},
    {"checked", (getter)menu_item_get_checked, (setter)menu_item_set_checked, NULL, NULL},
    {"radio", (getter)menu_item_get_radio, (setter)menu_item_set_radio, NULL, NULL},
    {"enabled", (getter)menu_item_get_enabled, (setter)menu_item_set_enabled, NULL, NULL},
    {"type", (getter)menu_item_get_type, (setter)NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static void
menu_item_dealloc(MenuItemObject *self) {
    if (self->id) {
        idm_delete_id(pwt_globals.menu_item_idm, self->id);
        self->id = 0;
    }
    Py_XDECREF(self->string);
    Py_XDECREF(self->callback);
    Py_XDECREF(self->sub);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject*
menu_item_repr(MenuItemObject *self) {
    switch (self->type) {
        case MENU_ITEM_TYPE_SEPARATOR:
            return PyUnicode_FromString("<MenuItem.separator()>");
        case MENU_ITEM_TYPE_STRING:
            return PyUnicode_FromFormat("<MenuItem.string(label=%R)>", self->string);
        case MENU_ITEM_TYPE_CHECK:
            return PyUnicode_FromFormat("<MenuItem.check(label=%R)>", self->string);
        case MENU_ITEM_TYPE_SUBMENU:
            if (!self->sub) {
                return PyUnicode_FromFormat("<MenuItem.submenu(<NULL>, label=%R)>", self->string);
            }
            return PyUnicode_FromFormat("<MenuItem.submenu(%R, label=%R)>", self->sub, self->string);
        default:
            PyErr_Format(PyExc_SystemError, "Unknown menu item type %d", self->type);
            return NULL;
    }
}

PyTypeObject *
create_menu_item_type(PyObject *module) {
    static PyType_Spec menu_item_metaclass_spec;

    PyType_Slot menu_item_metaclass_slots[] = {
        {Py_tp_methods, menu_item_metaclass_methods},
        {0, NULL}
    };

    menu_item_metaclass_spec.name = "pywintray._MenuItemMetaclass";
    menu_item_metaclass_spec.basicsize = sizeof(PyHeapTypeObject);
    menu_item_metaclass_spec.itemsize = 0;
    menu_item_metaclass_spec.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE;
    menu_item_metaclass_spec.slots = menu_item_metaclass_slots;

    PyObject *menu_item_metaclass = PyType_FromModuleAndSpec(
        module, &menu_item_metaclass_spec,
        (PyObject *)(&PyType_Type)
    );
    if(!menu_item_metaclass) {
        return NULL;
    }

    static PyType_Spec menu_item_spec;

    PyType_Slot menu_item_slots[] = {
        {Py_tp_methods, menu_item_methods},
        {Py_tp_getset, menu_item_getset},
        {Py_tp_repr, menu_item_repr},
        {Py_tp_dealloc, menu_item_dealloc},
        {0, NULL}
    };

    menu_item_spec.name = "pywintray.MenuItem";
    menu_item_spec.basicsize = sizeof(MenuItemObject);
    menu_item_spec.itemsize = 0;
    menu_item_spec.flags = Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_DISALLOW_INSTANTIATION |
        Py_TPFLAGS_IMMUTABLETYPE;
    menu_item_spec.slots = menu_item_slots;

    PyObject *menu_item_class = PyType_FromModuleAndSpec(
        module, &menu_item_spec, NULL
    );
    // See the comments about PyType_FromMetaclass in create_menu_type

    // replace the ob_type
    if (menu_item_class) {
        Py_DECREF(menu_item_class->ob_type);
        menu_item_class->ob_type = (PyTypeObject *)menu_item_metaclass;
        Py_INCREF(menu_item_class->ob_type);
    }

    // clean up
    Py_DECREF(menu_item_metaclass);

    return (PyTypeObject *)menu_item_class;
}
