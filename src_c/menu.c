/*
This file implements the pywintray.Menu class
*/

#include "pywintray.h"

static int
menu_metaclass_setattr(MenuTypeObject *self, char *attr, PyObject *value) {
    PyErr_SetString(PyExc_AttributeError, "This class is immutable");
    return -1;
}

void
menu_metaclass_dealloc(MenuTypeObject *self) {
    Py_XDECREF(self->items_list);
    self->items_list = NULL;
    if(self->handle) {
        DestroyMenu(self->handle);
        self->handle = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

BOOL
menu_subtype_check(PyObject *arg) {
    if(!PyType_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a type object");
        return FALSE;
    }
    if(!PyType_IsSubtype((PyTypeObject *)arg, (PyTypeObject *)(&MenuType)) || arg==(PyObject *)(&MenuType)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a subtype of Menu");
        return FALSE;
    }
    return TRUE;
}

static BOOL
update_all_items_in_menu(MenuTypeObject *cls, BOOL insert) {
    for(Py_ssize_t i=0;i<PyList_GET_SIZE(cls->items_list);i++) {
        if(!update_menu_item(cls->handle, (UINT)i, (MenuItemObject *)PyList_GET_ITEM(cls->items_list, i), insert)) {
            return FALSE;
        }
    }
    return TRUE;
}

PyObject*
menu_init_subclass(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    cls->items_list = NULL;
    cls->handle = NULL;

    // the class should not be subtyped any more
    ((PyTypeObject *)cls)->tp_flags &= ~(Py_TPFLAGS_BASETYPE);

    // warning: tp_dict is read-only
    PyObject *class_dict = ((PyTypeObject *)cls)->tp_dict;

    cls->items_list = PyList_New(0);
    if (cls->items_list==NULL) {
        return NULL;
    }

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(class_dict, &pos, &key, &value)) {
        int is_menu_item = PyObject_IsInstance(value, (PyObject *)&MenuItemType);
        if (is_menu_item<0) {
            return NULL;
        }
        if(is_menu_item) {
            if (PyList_Append(cls->items_list, value)<0) {
                return NULL;
            }
        }
    }

    cls->handle = CreatePopupMenu();
    if (cls->handle==NULL) {
        RAISE_LAST_ERROR();
        return NULL;
    }

    if(!update_all_items_in_menu(cls, TRUE)) {
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject*
menu_popup(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    if (!cls->handle) {
        PyErr_SetString(PyExc_SystemError, "Invalid menu handle");
        return NULL;
    }

    if (!update_all_items_in_menu(cls, FALSE)) {
        return NULL;
    }

    POINT pos;
    if(!GetCursorPos(&pos)) {
        RAISE_LAST_ERROR();
        return NULL;
    }

    HWND tmp_window = CreateWindowEx(
        0, MESSAGE_WINDOW_CLASS_NAME, TEXT(""), WS_DISABLED, 
        0,0,0,0,NULL,NULL,NULL,NULL
    );

    if (!tmp_window) {
        RAISE_LAST_ERROR();
        return NULL;
    }

    BOOL result;
    Py_BEGIN_ALLOW_THREADS
    result = TrackPopupMenuEx(cls->handle, TPM_RETURNCMD|TPM_NONOTIFY, pos.x, pos.y, tmp_window, NULL);
    Py_END_ALLOW_THREADS

    DestroyWindow(tmp_window);

    if(!result) {
        Py_RETURN_NONE;
    }

    MenuItemObject *clicked_menu_item = idm_get_data_by_id(menu_item_idm, result);
    if (!clicked_menu_item) {
        return NULL;
    }
    if (clicked_menu_item->type!=MENU_ITEM_TYPE_STRING &&
        clicked_menu_item->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_SystemError, "This type of menu item doesn't have a callback");
        return NULL;
    }
    if (clicked_menu_item->string_check_data.callback) {
        if (!PyObject_CallNoArgs(clicked_menu_item->string_check_data.callback)) {
            return NULL;
        }
    }

    Py_RETURN_NONE;
}

PyObject*
menu_as_tuple(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    return PyList_AsTuple(cls->items_list);
}

static PyMethodDef menu_methods[] = {
    {"__init_subclass__", (PyCFunction)menu_init_subclass, METH_NOARGS|METH_CLASS, NULL},
    {"popup", (PyCFunction)menu_popup, METH_NOARGS|METH_CLASS, NULL},
    {"as_tuple", (PyCFunction)menu_as_tuple, METH_NOARGS|METH_CLASS, NULL},
    {NULL, NULL, 0, NULL}
};

MenuTypeObject MenuType = {
    .type = {
        PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "pywintray.Menu",
        .tp_doc = NULL,
        .tp_basicsize = sizeof(PyObject),
        .tp_itemsize = 0,
        .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
        .tp_methods = menu_methods
    }
};

BOOL
init_menu_class(PyObject *module) {
    static PyType_Spec spec;

    PyType_Slot menu_metaclass_slots[] = {
        {Py_tp_setattr, menu_metaclass_setattr},
        {Py_tp_dealloc, menu_metaclass_dealloc},
        {0, NULL}
    };

    spec.name = "pywintray.MenuMetaclass";
    spec.basicsize = sizeof(MenuTypeObject);
    spec.itemsize = 0;
    spec.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
    spec.slots = menu_metaclass_slots;

    MenuTypeObject *menu_metaclass = (MenuTypeObject *)PyType_FromModuleAndSpec(
        module, &spec,
        (PyObject *)(&PyType_Type)
    );
    if(!menu_metaclass) {
        return FALSE;
    }

    ((PyObject *)(&MenuType))->ob_type = (PyTypeObject *)menu_metaclass;
    return TRUE;
}
