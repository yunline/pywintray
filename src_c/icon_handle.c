/*
This file implements the pywintray.IconHandle class
*/

#include "pywintray.h"

static PyMethodDef icon_handle_methods[] = {
    {NULL, NULL, 0, NULL}
};

static PyObject *
icon_handle_new(PyTypeObject *cls, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"value", NULL};

    PyObject *arg;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &arg)) {
        return NULL;
    }

    if(!PyLong_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Value must be int");
    }

    HICON icon_handle = (HICON)PyLong_AsVoidPtr(arg);

    if (icon_handle==NULL)
    {
        if(PyErr_Occurred()) {
            return NULL;
        }
        PyErr_SetString(PyExc_ValueError, "'Invalid handle (NULL)");
        return NULL;
    }

    return (PyObject *)new_icon_handle(icon_handle, FALSE);
}

static void
icon_handle_dealloc(IconHandleObject *self) {
    if(self->need_free) {
        DestroyIcon(self->icon_handle);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

IconHandleObject *
new_icon_handle(HICON icon_handle, BOOL need_free) {
    PyTypeObject *cls = pwt_globals.IconHandleType;
    IconHandleObject *self = (IconHandleObject *)(cls->tp_alloc(cls, 0));
    self->icon_handle = icon_handle;
    self->need_free = need_free;
    return self;
}

PyTypeObject *
create_icon_handle_type(PyObject *module) {
    static PyType_Spec spec;

    PyType_Slot icon_handle_slots[] = {
        {Py_tp_methods, icon_handle_methods},
        {Py_tp_new, icon_handle_new},
        {Py_tp_dealloc, icon_handle_dealloc},
        {0, NULL}
    };

    spec.name = "pywintray.IconHandle";
    spec.basicsize = sizeof(IconHandleObject);
    spec.itemsize = 0;
    spec.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE;
    spec.slots = icon_handle_slots;

    return (PyTypeObject *)PyType_FromModuleAndSpec(module, &spec, NULL);
}
