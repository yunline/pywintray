#include "pywintray.h"

static const char menu_class_code[] = \
"class Meta(type):\n"
" def __setattr__(*_):raise AttributeError('This class is immutable')\n" \
"class Menu(metaclass=Meta):\n" \
" def __new__(*_):raise TypeError('Creating an instance for this class is not supported')\n" \
" __init_subclass__=(lambda:((_:=_menu_init_subclass,lambda c:_(c)))[1])()";

PyObject*
menu_init_subclass(PyObject *self, PyObject *arg) {
    if(!PyType_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a type object");
        return NULL;
    }
    
    // the class should not be subtyped any more
    ((PyTypeObject *)arg)->tp_flags &= ~(Py_TPFLAGS_BASETYPE);

    // warning: tp_dict is read-only
    PyObject *class_dict = ((PyTypeObject *)arg)->tp_dict;

    
    // Add capsule to the __dict__ of the class
    if (PyObject_GenericSetAttr(arg, Py_BuildValue("s", "_menu_capsule"), PyLong_FromLong(114514))<0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
init_menu_class(PyObject *module) {

    // Borrowed, don't DECREF
    PyObject *globals = PyModule_GetDict(module);
    if(!globals) {
        return NULL;
    }

    PyObject *locals = PyDict_New();
    if(!locals) {
        return NULL;
    }

    if(!PyRun_String(menu_class_code, Py_file_input, globals, locals)) {
        Py_DECREF(locals);
        return NULL;
    }
    PyObject *menu_class = PyDict_GetItemString(locals, "Menu");
    Py_DECREF(locals);
    return menu_class;
}