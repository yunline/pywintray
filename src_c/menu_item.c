/*
This file implements the pywintray.MenuItem class
*/

#include "pywintray.h"

static UINT menu_item_id_counter = 1;

PyObject *menu_item_id_dict = NULL;

static MenuItemObject *
new_menu_item() {
    MenuItemObject *self = (MenuItemObject *)MenuItemType.tp_alloc(&MenuItemType, 0);
    if (self) {
        self->type = MENU_ITEM_TYPE_NULL;
        self->string = NULL;
        self->sub = NULL;
    }
    return self;
}

static PyObject *
menu_item_separator(PyObject *cls, PyObject *args) {
    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_SEPARATOR;
    return (PyObject *)self;
}

static PyObject *
menu_item_string(PyObject *cls, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"string", NULL};

    PyObject *string_obj = NULL;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U", kwlist, &string_obj)) {
        return NULL;
    }

    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_STRING;
    self->string = string_obj;
    Py_INCREF(string_obj);

    return (PyObject *)self;
}

static PyObject *
menu_item_sbumenu(PyObject *cls, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"string", NULL};

    PyObject *string_obj = NULL;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U", kwlist, &string_obj)) {
        return NULL;
    }

    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }

    PyObject *decorator = PyObject_GetAttrString((PyObject *)self, "_submenu_decorator");
    if (!decorator) {
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_SUBMENU;
    self->string = string_obj;
    
    Py_INCREF(string_obj);

    return decorator;
}

static PyObject *
menu_item_submenu_decorator(MenuItemObject* self, PyObject *arg) {
    if(self->type!=MENU_ITEM_TYPE_SUBMENU) {
        PyErr_SetString(PyExc_TypeError, "This method is for submenu only");
        return NULL;
    }

    if(!menu_subtype_check(arg)) {
        return NULL;
    }

    self->sub = arg;
    
    Py_INCREF(arg);
    Py_INCREF(self);

    return (PyObject *)self;
}

static PyObject *
menu_item_register_callback(PyObject* self, PyObject *arg) {
    Py_RETURN_NONE;
}

static PyMethodDef menu_item_methods[] = {
    // class methods
    {"separator", (PyCFunction)menu_item_separator, METH_NOARGS|METH_CLASS, NULL},
    {"string", (PyCFunction)menu_item_string, METH_VARARGS|METH_KEYWORDS|METH_CLASS, NULL},
    {"submenu", (PyCFunction)menu_item_sbumenu, METH_VARARGS|METH_KEYWORDS|METH_CLASS, NULL},
    // internal methods
    {"_submenu_decorator", (PyCFunction)menu_item_submenu_decorator, METH_O, NULL},
    // methods
    {"register_callback", (PyCFunction)menu_item_register_callback, METH_O, NULL},
    {NULL, NULL, 0, NULL}
};

static void
menu_item_dealloc(MenuItemObject *self) {
    Py_XDECREF(self->string);
    Py_XDECREF(self->sub);
}

static PyObject*
menu_item_repr(MenuItemObject *self) {
    const char *type_string;
    switch (self->type)
    {
        case MENU_ITEM_TYPE_NULL:
            type_string="null";
            goto end_no_arg;
        case MENU_ITEM_TYPE_SEPARATOR:
            type_string="separator";
            goto end_no_arg;
        case MENU_ITEM_TYPE_STRING:
            type_string="string";
            goto end_string;
        case MENU_ITEM_TYPE_CHECK:
            type_string="check";
            goto end_string;
        case MENU_ITEM_TYPE_SUBMENU:
            type_string="submenu";
            goto end_string;
        default:
            PyErr_Format(PyExc_SystemError, "Unknown menu item type %d", self->type);
            return NULL;
    }

end_no_arg:
    return PyUnicode_FromFormat("<MenuItem(type=%s)>", type_string);
end_string:
    return PyUnicode_FromFormat("<MenuItem(type=%s, string=%R)>", type_string, self->string);
}

PyTypeObject MenuItemType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pywintray.MenuItem",
    .tp_doc = NULL,
    .tp_basicsize = sizeof(MenuItemObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)menu_item_dealloc,
    .tp_repr = (reprfunc)menu_item_repr,
    .tp_methods = menu_item_methods
};
