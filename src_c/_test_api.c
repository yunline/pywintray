#include "pywintray.h"

static PyObject*
test_api_get_internal_tray_icon_dict(PyObject* self, PyObject* args) {
    idm_mutex_acquire(pwt_globals.tray_icon_idm);
    PyObject *result = PyDictProxy_New(_idm_get_internal_dict(pwt_globals.tray_icon_idm));
    idm_mutex_release(pwt_globals.tray_icon_idm);
    return result;
}

static PyObject*
test_api_get_internal_menu_item_dict(PyObject* self, PyObject* args) {
    idm_mutex_acquire(pwt_globals.menu_item_idm);
    PyObject *result = PyDictProxy_New(_idm_get_internal_dict(pwt_globals.menu_item_idm));
    idm_mutex_release(pwt_globals.menu_item_idm);
    return result;
}

static PyObject*
test_api_get_internal_id(PyObject* self, PyObject* arg) {
    int result;

    result = PyObject_IsInstance(arg, (PyObject *)(&MenuItemType));
    if (result<0) {
        return NULL;
    }
    if (result) {
        return PyLong_FromUnsignedLong(((MenuItemObject *)arg)->id);
    }

    result = PyObject_IsInstance(arg, (PyObject *)(&TrayIconType));
    if (result<0) {
        return NULL;
    }
    if (result) {
        return PyLong_FromUnsignedLong(((TrayIconObject *)arg)->id);
    }

    result = PyObject_IsInstance(arg, (PyObject *)(&IconHandleType));
    if (result<0) {
        return NULL;
    }
    if (result) {
        return PyLong_FromVoidPtr((void *)(((IconHandleObject *)arg)->icon_handle));
    }

    if(menu_subtype_check((PyObject *)arg)) {
        return PyLong_FromVoidPtr(((MenuTypeObject *)arg)->handle);
    }
    // menu_subtype_check will set an error
    // which is not needed here
    PyErr_Clear();

    PyErr_SetString(PyExc_TypeError, "Can't get internal id for this type");
    return NULL;
}

static PyMethodDef test_api_methods[] = {
    {"get_internal_tray_icon_dict", (PyCFunction)test_api_get_internal_tray_icon_dict, METH_NOARGS, NULL},
    {"get_internal_menu_item_dict", (PyCFunction)test_api_get_internal_menu_item_dict, METH_NOARGS, NULL},
    {"get_internal_id", (PyCFunction)test_api_get_internal_id, METH_O, NULL},
    {NULL, NULL, 0, NULL}
};

PyObject *
create_test_api() {
    static PyType_Spec spec;
    PyType_Slot test_api_slots[] = {
        {Py_tp_methods, test_api_methods},
        {0, NULL}
    };
    spec.name = "pywintray._TestApi";
    spec.basicsize = sizeof(PyObject);
    spec.itemsize = 0;
    spec.flags = Py_TPFLAGS_DEFAULT;
    spec.slots = test_api_slots;

    PyTypeObject *test_api_class = (PyTypeObject *)PyType_FromSpec(&spec);
    if (!test_api_class) {
        return NULL;
    }
    
    PyObject *new_args = Py_BuildValue("()");
    PyObject *new_kwargs = Py_BuildValue("{}");
    PyObject *test_api_object = test_api_class->tp_new(test_api_class, new_args, new_kwargs);
    Py_DECREF(new_args);
    Py_DECREF(new_kwargs);

    return test_api_object;
}