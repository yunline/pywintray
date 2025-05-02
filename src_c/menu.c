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

}

PyObject *
menu_new(PyTypeObject *cls, PyObject *args, PyObject *kwargs) {
    PyErr_SetString(PyExc_TypeError, "Creating an instance for this class is not supported");
    return NULL;
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

PyObject*
menu_init_subclass(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    // the class should not be subtyped any more
    ((PyTypeObject *)cls)->tp_flags &= ~(Py_TPFLAGS_BASETYPE);

    // warning: tp_dict is read-only
    PyObject *class_dict = ((PyTypeObject *)cls)->tp_dict;

    Py_RETURN_NONE;
}

PyObject*
menu_popup(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject*
menu_as_tuple(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyMethodDef menu_methods[] = {
    {"__init_subclass__", (PyCFunction)menu_init_subclass, METH_NOARGS|METH_CLASS, NULL},
    {"popup", (PyCFunction)menu_popup, METH_NOARGS|METH_CLASS, NULL},
    {"as_tuple", (PyCFunction)menu_as_tuple, METH_NOARGS|METH_CLASS, NULL},
    {NULL, NULL, 0, NULL}
};

MenuTypeObject MenuType = {
    .ob_base = {
        PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "pywintray.Menu",
        .tp_doc = NULL,
        .tp_basicsize = sizeof(PyObject),
        .tp_itemsize = 0,
        .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
        .tp_new = menu_new,
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
