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
    HANDLE mutex;
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
    idm->mutex = CreateMutex(NULL, FALSE, NULL);
    if (!idm->mutex) {
        RAISE_LAST_ERROR();
        Py_DECREF(idm->dict);
        idm_free(idm);
        return NULL;
    }
    return idm;
}

void
idm_delete(IDManager *idm) {
    CloseHandle(idm->mutex);
    Py_DECREF(idm->dict);
    idm_free(idm);
}

void
idm_mutex_acquire(IDManager *idm) {
    WaitForSingleObject(idm->mutex, INFINITE);
}

void
idm_mutex_release(IDManager *idm) {
    ReleaseMutex(idm->mutex);
}

UINT
idm_allocate_id(IDManager *idm, void *data) {
    idm_mutex_acquire(idm);

    UINT id = idm->id_counter;

    PyObject *key_obj = PyLong_FromUnsignedLong(id);
    if(key_obj==NULL) {
        goto fail;
    }
    PyObject *value_obj = PyCapsule_New(data, NULL, NULL);
    if(value_obj==NULL) {
        Py_DECREF(key_obj);
        goto fail;
    }
    int result = PyDict_SetItem(idm->dict, key_obj, value_obj);
    Py_DECREF(key_obj);
    Py_DECREF(value_obj);

    idm->id_counter++;

    if(result<0) {
        goto fail;
    }
    
    idm_mutex_release(idm);
    return id;

fail:
    idm_mutex_release(idm);
    return 0;
}

void *
idm_get_data_by_id(IDManager *idm, UINT id) {
    PyObject *key_obj = PyLong_FromUnsignedLong(id);
    if(key_obj==NULL) {
        return NULL;
    }

    idm_mutex_acquire(idm);
    
    PyObject *capsule = PyDict_GetItemWithError(idm->dict, key_obj);
    Py_DECREF(key_obj);
    if(!capsule) {
        idm_mutex_release(idm);
        return NULL;
    }
    void *result = PyCapsule_GetPointer(capsule, NULL);

    idm_mutex_release(idm);

    return result;
}

BOOL
idm_delete_id(IDManager *idm, UINT id) {
    PyObject *key_obj = PyLong_FromUnsignedLong(id);
    if(key_obj==NULL) {
        return FALSE;
    }

    idm_mutex_acquire(idm);

    int result = PyDict_DelItem(idm->dict, key_obj);

    idm_mutex_release(idm);

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

PyObject *
_idm_get_internal_dict(IDManager *idm) {
    return idm->dict;
}

