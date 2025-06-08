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
    CRITICAL_SECTION cs;
    IDMFlags flags;
};

IDManager *
idm_new(IDMFlags flags) {
    IDManager *idm = idm_malloc(sizeof(IDManager));
    idm->dict = PyDict_New();
    if (!idm->dict) {
        idm_free(idm);
        return NULL;
    }
    idm->id_counter = 1;
    InitializeCriticalSection(&(idm->cs));
    idm->flags = flags;
    return idm;
}

void
idm_delete(IDManager *idm) {
    DeleteCriticalSection(&(idm->cs));
    Py_DECREF(idm->dict);
    idm_free(idm);
}

void
idm_enter_critical_section(IDManager *idm) {
    EnterCriticalSection(&(idm->cs));
}

void
idm_leave_critical_section(IDManager *idm) {
    LeaveCriticalSection(&(idm->cs));
}

UINT
idm_allocate_id(IDManager *idm, void *data) {
    idm_enter_critical_section(idm);

    if (!(idm->flags&IDM_FLAGS_ALLOCATE_ID)) {
        PyErr_SetString(PyExc_SystemError, "This idm doesn't support allocate_id");
        goto fail;
    }

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
    
    idm_leave_critical_section(idm);
    return id;

fail:
    idm_leave_critical_section(idm);
    return 0;
}

BOOL
idm_put_id(IDManager *idm, UINT id, void *data) {
    idm_enter_critical_section(idm);

    if (idm->flags&IDM_FLAGS_ALLOCATE_ID) {
        PyErr_SetString(PyExc_SystemError, "This idm doesn't support put_id");
        goto fail;
    }

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

    if(result<0) {
        goto fail;
    }

    idm_leave_critical_section(idm);
    return TRUE;

fail:
    idm_leave_critical_section(idm);
    return FALSE;
}

void *
idm_get_data_by_id(IDManager *idm, UINT id) {
    PyObject *key_obj = PyLong_FromUnsignedLong(id);
    if(key_obj==NULL) {
        return NULL;
    }

    idm_enter_critical_section(idm);
    
    PyObject *capsule = PyDict_GetItemWithError(idm->dict, key_obj);
    Py_DECREF(key_obj);
    if(!capsule) {
        idm_leave_critical_section(idm);
        return NULL;
    }
    void *result = PyCapsule_GetPointer(capsule, NULL);

    idm_leave_critical_section(idm);

    return result;
}

BOOL
idm_delete_id(IDManager *idm, UINT id) {
    PyObject *key_obj = PyLong_FromUnsignedLong(id);
    if(key_obj==NULL) {
        return FALSE;
    }

    idm_enter_critical_section(idm);

    int result = PyDict_DelItem(idm->dict, key_obj);

    idm_leave_critical_section(idm);

    Py_DECREF(key_obj);

    if (result<0) {
        return FALSE;
    }
    return TRUE;
}

int
idm_next(IDManager *idm, Py_ssize_t *ppos, UINT *pid, void **pdata) {
    PyObject *value_obj;
    PyObject *key_obj;
    int result = PyDict_Next(idm->dict, ppos, &key_obj, &value_obj);
    if(result) {
        if (pdata) {
            *pdata = PyCapsule_GetPointer(value_obj, NULL);
        }
        if (pid) {
            *pid = PyLong_AsUnsignedLong(key_obj);
        }
    }
    return result;
}

PyObject *
_idm_get_internal_dict(IDManager *idm) {
    return idm->dict;
}

