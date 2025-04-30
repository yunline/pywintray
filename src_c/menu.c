/*
This file implements the pywintray.Menu class
*/

#include "pywintray.h"

PyTypeObject *pMenuType = NULL;

static const char menu_class_code[] = \
// indent: 1 Space
// namespace: class
// toplevel vars will be written into locals
"class Meta(type):\n" \
" def __setattr__(*_):raise AttributeError('This class is immutable')\n" \
"class Menu(metaclass=Meta):\n" \
" def __new__(*_):raise TypeError('Creating an instance for this class is not supported')\n" \
// _menu_init_subclass will be delete from the globals later
// so we wrap it with the first lambda to make it a closure
// _menu_init_subclass is built-in function, which is not a descriptor
// so we wrap it with the second lambda to make it bahave like a normal function
" __init_subclass__=(lambda f:lambda c:f(c))("MENU_INIT_SUBCLASS_TMP_NAME")\n"\
// popup is same as __init_subclass__ but requires a classmethod wrapper
" popup=classmethod((lambda f:lambda c:f(c))("MENU_POPUP_TMP_NAME"))";

BOOL
menu_subtype_check(PyObject *arg) {
    if(!pMenuType) {
        PyErr_SetString(PyExc_SystemError, "pMenuType is NULL");
        return FALSE;
    }
    if(!PyType_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a type object");
        return FALSE;
    }
    if(!PyType_IsSubtype((PyTypeObject *)arg, pMenuType) || arg==(PyObject *)pMenuType) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a subtype of Menu");
        return FALSE;
    }
    return TRUE;
}

void
menu_capsule_destruct(PyObject *capsule) {
    MenuInternals *menu_internals = (MenuInternals *)PyCapsule_GetPointer(capsule, NULL);
    if (menu_internals) {
        PyMem_RawFree(menu_internals);
    }
}

PyObject*
menu_init_subclass(PyObject *self, PyObject *arg) {
    if(!menu_subtype_check(arg)) {
        return NULL;
    }
    
    // the class should not be subtyped any more
    ((PyTypeObject *)arg)->tp_flags &= ~(Py_TPFLAGS_BASETYPE);

    // warning: tp_dict is read-only
    PyObject *class_dict = ((PyTypeObject *)arg)->tp_dict;

    if (PyDict_GetItemString(class_dict, MENU_CAPSULE_NAME)) {
        PyErr_SetString(PyExc_TypeError, "Name '"MENU_CAPSULE_NAME"' is preserved for internal use");
        return NULL;
    }

    MenuInternals *menu_internals = (MenuInternals *)PyMem_RawMalloc(sizeof(MenuInternals));
    if(!menu_internals) {
        return PyErr_NoMemory();
    }

    PyObject *capsule = PyCapsule_New(menu_internals, NULL, (PyCapsule_Destructor)menu_capsule_destruct);
    if(!capsule) {
        PyMem_RawFree(menu_internals);
        return NULL;
    }
    
    // Add capsule to the __dict__ of the class
    if (PyObject_GenericSetAttr(arg, Py_BuildValue("s", MENU_CAPSULE_NAME), capsule)<0) {
        // menu_internals is freed by capsule destructor
        Py_DECREF(capsule);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject*
menu_popup(PyObject *self, PyObject *arg) {
    if(!menu_subtype_check(arg)) {
        return NULL;
    }

    Py_RETURN_NONE;
}

PyTypeObject *
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
    if (!menu_class) {
        return NULL;
    }
    if (!PyType_Check(menu_class)) {
        Py_DECREF(menu_class);
        PyErr_SetString(PyExc_SystemError, "Menu is not a class");
        return NULL;
    }
    return (PyTypeObject *)menu_class;
}
