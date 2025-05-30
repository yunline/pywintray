#include "pywintray.h"

static PyObject*
test_api_get_internal_tray_icon_dict(PyObject* self, PyObject* args) {
    return PyDictProxy_New(_idm_get_internal_dict(tray_icon_idm));
}

static PyMethodDef test_api_methods[] = {
    {"get_internal_tray_icon_dict", (PyCFunction)test_api_get_internal_tray_icon_dict, METH_NOARGS, NULL},
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