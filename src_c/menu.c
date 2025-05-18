/*
This file implements the pywintray.Menu class
*/

#include "pywintray.h"

static int
menu_metaclass_setattr(MenuTypeObject *self, char *attr, PyObject *value) {
    PyErr_SetString(PyExc_AttributeError, "This class doesn't support setting attribute");
    return -1;
}

static void
menu_metaclass_dealloc(MenuTypeObject *cls) {
    MENUITEMINFO info;
    info.cbSize = sizeof(MENUITEMINFO);
    info.fMask = MIIM_DATA;

    // free the item list
    Py_XDECREF(cls->items_list);

    // free the menu handle
    if(cls->handle) {
        // remove all items before destroy
        // to prevent recursive destroy
        int count = GetMenuItemCount(cls->handle);
        while (count--) {
            RemoveMenu(cls->handle, 0, MF_BYPOSITION);
        }
        // destroy the handle
        DestroyMenu(cls->handle);
    }
    
    // inherit from the PyType_Type
    PyType_Type.tp_dealloc((PyObject *)cls);
}

BOOL
menu_subtype_check(PyObject *arg) {
    if(!PyType_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a type object");
        return FALSE;
    }
    if(!PyType_IsSubtype((PyTypeObject *)arg, (PyTypeObject *)(&MenuType)) || arg==(PyObject *)(&MenuType)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a subtype of Menu");
        return FALSE;
    }
    return TRUE;
}

static PyObject*
menu_init_subclass(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    cls->items_list = NULL;
    cls->handle = NULL;
    cls->parent_window = NULL;

    // the class should not be subtyped any more
    ((PyTypeObject *)cls)->tp_flags &= ~(Py_TPFLAGS_BASETYPE);

    // warning: tp_dict is read-only
    PyObject *class_dict = ((PyTypeObject *)cls)->tp_dict;

    cls->items_list = PyList_New(0);
    if (cls->items_list==NULL) {
        return NULL;
    }

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(class_dict, &pos, &key, &value)) {
        int is_menu_item = PyObject_IsInstance(value, (PyObject *)&MenuItemType);
        if (is_menu_item<0) {
            return NULL;
        }
        if(is_menu_item) {
            if (PyList_Append(cls->items_list, value)<0) {
                return NULL;
            }
        }
    }

    cls->handle = CreatePopupMenu();
    if (cls->handle==NULL) {
        RAISE_LAST_ERROR();
        return NULL;
    }

    if(!update_all_items_in_menu(cls, TRUE)) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject*
menu_popup(MenuTypeObject *cls, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {
        "position", 
        "allow_right_click", 
        "horizontal_align", 
        "vertical_align", 
        NULL
    };

    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    if (!cls->handle) {
        PyErr_SetString(PyExc_SystemError, "Invalid menu handle");
        return NULL;
    }
    
    PyObject *pos_obj=NULL, 
        *h_align_obj=NULL, 
        *v_align_obj=NULL, 
        *anim_obj=NULL;
    
    BOOL allow_right_click = FALSE;

    POINT pos;

    UINT flags = TPM_NONOTIFY|TPM_RETURNCMD;

    HWND parent_window = NULL;

    if(!PyArg_ParseTupleAndKeywords(
        args, kwargs, "|OpOO", kwlist,
        &pos_obj, &allow_right_click, 
        &h_align_obj, &v_align_obj
    )) {
        return NULL;
    }

    // handle argument 'position'
    if(!pos_obj || Py_IsNone(pos_obj)) {
        if(!GetCursorPos(&pos)) {
            RAISE_LAST_ERROR();
            return NULL;
        }
    }
    else if (
        PyTuple_Check(pos_obj) && 
        PyTuple_GET_SIZE(pos_obj)==2 && 
        PyLong_Check(PyTuple_GET_ITEM(pos_obj, 0)) &&
        PyLong_Check(PyTuple_GET_ITEM(pos_obj, 1))
    ) {
        pos.x = PyLong_AsLong(PyTuple_GET_ITEM(pos_obj, 0));
        pos.y = PyLong_AsLong(PyTuple_GET_ITEM(pos_obj, 1));
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Argument 'position' must be a tuple[int, int]");
        return NULL;
    }

    // handle argument 'allow_right_click'
    if (allow_right_click) {
        flags |= TPM_RIGHTBUTTON;
    }

    // handle argument 'horizontal_align'
    if (!h_align_obj) {
        goto h_align_default;
    }
    if (!PyUnicode_Check(h_align_obj)) {
        PyErr_SetString(PyExc_TypeError, "Argument 'horizontal_align' must be a str");
        return NULL;
    }
    if (PyUnicode_EqualToUTF8(h_align_obj, "left")) {
h_align_default:
        flags |= TPM_LEFTALIGN;
    }
    else if (PyUnicode_EqualToUTF8(h_align_obj, "center")) {
        flags |= TPM_CENTERALIGN;
    }
    else if (PyUnicode_EqualToUTF8(h_align_obj, "right")) {
        flags |= TPM_RIGHTALIGN;
    }
    else {
        PyErr_SetString(PyExc_ValueError, "Value of 'horizontal_align' must in ['left', 'center', 'right']");
        return NULL;
    }


    // handle argument 'vertical_align'
    if (!v_align_obj) {
        goto v_align_default;
    }
    if (!PyUnicode_Check(v_align_obj)) {
        PyErr_SetString(PyExc_TypeError, "Argument 'vertical_align' must be a str");
        return NULL;
    }
    if (PyUnicode_EqualToUTF8(v_align_obj, "top")) {
v_align_default:
        flags |= TPM_TOPALIGN;
    }
    else if (PyUnicode_EqualToUTF8(v_align_obj, "center")) {
        flags |= TPM_VCENTERALIGN;
    }
    else if (PyUnicode_EqualToUTF8(v_align_obj, "bottom")) {
        flags |= TPM_BOTTOMALIGN;
    }
    else {
        PyErr_SetString(PyExc_ValueError, "Value of 'vertical_align' must in ['top', 'center', 'bottom']");
        return NULL;
    }

    // update menu item data
    if (!update_all_items_in_menu(cls, FALSE)) {
        return NULL;
    }

    parent_window = CreateWindowEx(
        0, MESSAGE_WINDOW_CLASS_NAME, TEXT(""), WS_DISABLED, 
        0,0,0,0,NULL,NULL,NULL,NULL
    );
    if (!parent_window) {
        RAISE_LAST_ERROR();
        return NULL;
    }

    // store parent window
    cls->parent_window = parent_window;

    BOOL result;
    Py_BEGIN_ALLOW_THREADS
    SetForegroundWindow(parent_window);

    // track menu
    result = TrackPopupMenuEx(cls->handle, flags, pos.x, pos.y, parent_window, NULL);

    PostMessage(parent_window, WM_NULL, 0, 0);
    Py_END_ALLOW_THREADS

    // clear parent window
    cls->parent_window = NULL;

    // clean up
    DestroyWindow(parent_window);

    if(!result) {
        UINT error_code = GetLastError();
        if (error_code) {
            RAISE_WIN32_ERROR(error_code);
            return NULL;
        }
        Py_RETURN_NONE;
    }

    MenuItemObject *clicked_menu_item = idm_get_data_by_id(menu_item_idm, result);
    if (!clicked_menu_item) {
        return NULL;
    }
    if (clicked_menu_item->type!=MENU_ITEM_TYPE_STRING &&
        clicked_menu_item->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_SystemError, "This type of menu item doesn't have a callback");
        return NULL;
    }
    if (clicked_menu_item->string_check_data.callback) {
        if (!PyObject_CallOneArg(
            clicked_menu_item->string_check_data.callback, 
            (PyObject *)clicked_menu_item
        )) {
            return NULL;
        }
    }

    if (clicked_menu_item->type==MENU_ITEM_TYPE_CHECK) {
        // toggle check state
        clicked_menu_item->string_check_data.checked = \
            !(clicked_menu_item->string_check_data.checked);
        clicked_menu_item->update_counter++;
    }

    Py_RETURN_NONE;
}

static PyObject*
menu_close(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }
    if(!cls->parent_window) {
        Py_RETURN_NONE;
    }
    PostMessage(cls->parent_window, WM_CANCELMODE, 0, 0);
    Py_RETURN_NONE;
}

static PyObject*
menu_as_tuple(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    return PyList_AsTuple(cls->items_list);
}

static PyObject *
menu_insert_item(MenuTypeObject *cls, PyObject *args) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    PyObject *new_item;
    Py_ssize_t index;
    if (!PyArg_ParseTuple(args, "nO!", &index, &MenuItemType, &new_item)) {
        return NULL;
    }

    Py_ssize_t list_size = PyList_GET_SIZE(cls->items_list);

    // normalize index
    if (index<0) {
        index += list_size;
    }
    if (index<0) {
        index=0;
    }
    if (index>=list_size) {
        index = list_size;
    }

    if (PyList_Insert(cls->items_list, index, new_item)<0) {
        return NULL;
    }

    if(!update_menu_item(cls->handle, (UINT)index, (MenuItemObject *)new_item, TRUE)) {
        PySequence_DelItem(cls->items_list, index);
        return FALSE;
    }

    Py_RETURN_NONE;
}

static PyObject *
menu_remove_item(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }
    if (!PyLong_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Argument mus be an int");
        return NULL;
    }
    Py_ssize_t index = PyLong_AsSsize_t(arg);
    if (index==-1 && PyErr_Occurred()) {
        return NULL;
    }

    Py_ssize_t list_size = PyList_GET_SIZE(cls->items_list);

    // normalize index
    if (index<0) {
        index += list_size;
    }
    if (index<0 || index>=list_size) {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        return NULL;
    }

    MENUITEMINFO info;

    info.cbSize = sizeof(MENUITEMINFO);
    info.fMask = MIIM_DATA|MIIM_ID;

    if (!GetMenuItemInfo(cls->handle, (UINT)index, TRUE, &info)) {
        RAISE_LAST_ERROR();
        return FALSE;
    }

    MenuItemObject *item = (MenuItemObject *)PyList_GET_ITEM(cls->items_list, index);
    if (info.wID!=item->id) {
        PyErr_SetString(PyExc_SystemError, "Menu item id doesn't match");
        return FALSE;
    }

    if (!RemoveMenu(cls->handle, (UINT)index, MF_BYPOSITION)) {
        RAISE_LAST_ERROR();
        return NULL;
    }

    PWT_Free((void *)info.dwItemData);

    if (PySequence_DelItem(cls->items_list, index)<0) {
        PyErr_SetString(PyExc_SystemError, "Unable to delete item from internal list");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
menu_append_item(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    PyObject *args = Py_BuildValue("(iO)", PyList_GET_SIZE(cls->items_list), arg);
    if (!args) {
        return NULL;
    }
    PyObject *result = menu_insert_item(cls, args);
    Py_DECREF(args);
    return result;
}

static PyMethodDef menu_methods[] = {
    {"__init_subclass__", (PyCFunction)menu_init_subclass, METH_NOARGS|METH_CLASS, NULL},
    {"popup", (PyCFunction)menu_popup, METH_VARARGS|METH_KEYWORDS|METH_CLASS, NULL},
    {"close", (PyCFunction)menu_close, METH_NOARGS|METH_CLASS, NULL},
    {"as_tuple", (PyCFunction)menu_as_tuple, METH_NOARGS|METH_CLASS, NULL},
    {"insert_item", (PyCFunction)menu_insert_item, METH_VARARGS|METH_CLASS, NULL},
    {"remove_item", (PyCFunction)menu_remove_item, METH_O|METH_CLASS, NULL},
    {"append_item", (PyCFunction)menu_append_item, METH_O|METH_CLASS, NULL},
    {NULL, NULL, 0, NULL}
};

MenuTypeObject MenuType = {
    .type = {
        PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "pywintray.Menu",
        .tp_doc = NULL,
        .tp_basicsize = sizeof(PyObject),
        .tp_itemsize = 0,
        .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
        .tp_methods = menu_methods
    }
};

static PyObject *
menu_get_poped_up(MenuTypeObject *cls, void *closure) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    if(cls->parent_window) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyGetSetDef menu_metaclass_getset[] = {
    {"poped_up", (getter)menu_get_poped_up, (setter)NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};


BOOL
init_menu_class(PyObject *module) {
    static PyType_Spec spec;

    PyType_Slot menu_metaclass_slots[] = {
        {Py_tp_setattr, menu_metaclass_setattr},
        {Py_tp_dealloc, menu_metaclass_dealloc},
        {Py_tp_getset, menu_metaclass_getset},
        {0, NULL}
    };

    spec.name = "pywintray.MenuMetaclass";
    spec.basicsize = sizeof(MenuTypeObject);
    spec.itemsize = 0;
    spec.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
    spec.slots = menu_metaclass_slots;

    MenuTypeObject *menu_metaclass = (MenuTypeObject *)PyType_FromModuleAndSpec(
        module, &spec,
        (PyObject *)(&PyType_Type)
    );
    if(!menu_metaclass) {
        return FALSE;
    }

    ((PyObject *)(&MenuType))->ob_type = (PyTypeObject *)menu_metaclass;
    return TRUE;
}
