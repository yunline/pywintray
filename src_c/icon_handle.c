/*
This file implements the pywintray.IconHandle class
*/

#include "pywintray.h"


static PyObject*
icon_handle_from_int(PyObject *cls, PyObject *arg) {
    if(!PyLong_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be int");
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

static PyMethodDef icon_handle_methods[] = {
    {"from_int", (PyCFunction)icon_handle_from_int, METH_O|METH_CLASS, NULL},
    {NULL, NULL, 0, NULL}
};

static void
icon_handle_dealloc(IconHandleObject *self)
{
    if(self->need_free) {
        DestroyIcon(self->icon_handle);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

IconHandleObject *
new_icon_handle(HICON icon_handle, BOOL need_free)
{
    IconHandleObject *self = (IconHandleObject *)IconHandleType.tp_alloc(&IconHandleType, 0);
    self->icon_handle = icon_handle;
    self->need_free = need_free;
    return self;
}

PyTypeObject IconHandleType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pywintray.IconHandle",
    .tp_doc = NULL,
    .tp_basicsize = sizeof(IconHandleObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)icon_handle_dealloc,
    .tp_methods = icon_handle_methods,
};

