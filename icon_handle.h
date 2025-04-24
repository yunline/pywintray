#ifndef ICON_HANDLE_H
#define ICON_HANDLE_H

#include <Python.h>
#include <Windows.h>

typedef struct {
    PyObject_HEAD
    HICON icon_handle;
    BOOL need_free;
} IconHandleObject;

IconHandleObject *new_icon_handle(HICON icon_handle, BOOL need_free);

extern PyTypeObject IconHandleType;

#endif
