#include "pywintray.h"

#ifndef idm_malloc
#define idm_malloc(s) PyMem_RawMalloc(s)
#endif

#ifndef idm_free
#define idm_free(p) PyMem_RawFree(p)
#endif

struct IDManager {
    UINT id_counter;
    PyObject* dict;
};

IDManager *
idm_new() {
    IDManager *idm = idm_malloc(sizeof(IDManager));
    idm->dict = PyDict_New();
    if (!idm->dict) {
        idm_free(idm);
        return NULL;
    }
    idm->id_counter = 1;
    return idm;
}

void
idm_delete(IDManager *idm) {
    Py_DECREF(idm->dict);
    idm_free(idm);
}

UINT
idm_allocate_id(IDManager *idm, void *data) {
    UINT id = idm->id_counter;

    PyObject *key_obj = PyLong_FromUnsignedLong(id);
    if(key_obj==NULL) {
        return 0;
    }
    PyObject *value_obj = PyCapsule_New(data, NULL, NULL);
    if(value_obj==NULL) {
        Py_DECREF(key_obj);
        return 0;
    }
    int result = PyDict_SetItem(idm->dict, key_obj, value_obj);
    Py_DECREF(key_obj);
    Py_DECREF(value_obj);

    idm->id_counter++;

    if(result<0) {
        return 0;
    }

    return id;
}

void *
idm_get_data_by_id(IDManager *idm, UINT id) {
    PyObject *key_obj = PyLong_FromUnsignedLong(id);
    if(key_obj==NULL) {
        return NULL;
    }
    PyObject *capsule = PyDict_GetItemWithError(idm->dict, key_obj);
    Py_DECREF(key_obj);
    if(!capsule) {
        return NULL;
    }
    return PyCapsule_GetPointer(capsule, NULL);
}

BOOL
idm_delete_id(IDManager *idm, UINT id) {
    PyObject *key_obj = PyLong_FromUnsignedLong(id);
    if(key_obj==NULL) {
        return FALSE;
    }
    int result = PyDict_DelItem(idm->dict, key_obj);
    Py_DECREF(key_obj);
    if (result<0) {
        return FALSE;
    }
    return TRUE;
}

int
idm_next_data(IDManager *idm, Py_ssize_t *ppos, void **pdata) {
    PyObject *value;
    int result = PyDict_Next(idm->dict, ppos, NULL, &value);
    if(result) {
        *pdata = PyCapsule_GetPointer(value, NULL);
    }
    return result;
}

