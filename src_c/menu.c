/*
This file implements the pywintray.Menu class
*/

#include "pywintray.h"

static int
menu_metaclass_setattr(MenuTypeObject *self, char *attr, PyObject *value) {
    PyErr_SetString(PyExc_AttributeError, "This class is immutable");
    return -1;
}

void
menu_metaclass_dealloc(MenuTypeObject *cls) {
    MENUITEMINFO info;
    info.cbSize = sizeof(MENUITEMINFO);
    info.fMask = MIIM_DATA;

    // free user data of each items
    for(Py_ssize_t i=0;i<PyList_GET_SIZE(cls->items_list);i++) {
        MenuItemObject *item = (MenuItemObject *)PyList_GET_ITEM(cls->items_list, i);
        if (!GetMenuItemInfo(cls->handle, (UINT)item->id, FALSE, &info)) {
            continue; // ignore errors
        }
        PWT_Free((void *)info.dwItemData);
    }

    // free the item list
    Py_XDECREF(cls->items_list);

    // free the menu handle
    if(cls->handle) {
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

static BOOL
update_all_items_in_menu(MenuTypeObject *cls, BOOL insert) {
    for(Py_ssize_t i=0;i<PyList_GET_SIZE(cls->items_list);i++) {
        if(!update_menu_item(cls->handle, (UINT)i, (MenuItemObject *)PyList_GET_ITEM(cls->items_list, i), insert)) {
            return FALSE;
        }
    }
    return TRUE;
}

PyObject*
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

PyObject*
menu_popup(MenuTypeObject *cls, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {
        "position", 
        "allow_right_click", 
        "horizontal_align", 
        "vertical_align", 
        "parent_window", 
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
        *anim_obj=NULL, 
        *parent_win_obj=NULL;
    
    BOOL allow_right_click = FALSE;

    POINT pos;

    UINT flags = TPM_NONOTIFY|TPM_RETURNCMD;

    HWND parent_window = NULL;

    if(!PyArg_ParseTupleAndKeywords(
        args, kwargs, "|OpOOO", kwlist,
        &pos_obj, &allow_right_click, 
        &h_align_obj, &v_align_obj, &parent_win_obj
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

    #define ASSERT_UNICODE_TYPE(obj, argname) {\
        if (!PyUnicode_Check(obj)) { \
            PyErr_SetString(PyExc_TypeError, "Argument "argname" must be a str"); \
            return NULL; \
        } \
    }

    // handle argument 'horizontal_align'
    if (!h_align_obj) {
        goto h_align_default;
    }
    ASSERT_UNICODE_TYPE(h_align_obj, "'horizontal_align'");
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
    ASSERT_UNICODE_TYPE(v_align_obj, "'vertical_align'");
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

    #undef ASSERT_UNICODE_TYPE

    // handle argument 'parent_window'
    if (parent_win_obj && !Py_IsNone(parent_win_obj)) {
        if (!PyLong_Check(parent_win_obj)) {
            PyErr_SetString(PyExc_TypeError, "Argument 'parent_window' must be an int");
            return NULL;
        }
        parent_window = PyLong_AsVoidPtr(parent_win_obj);
        if (!parent_window){
            if(PyErr_Occurred()) {
                return NULL;
            }
            PyErr_SetString(PyExc_ValueError, "'Invalid handle (NULL)");
            return NULL;
        }
    }

    if (!update_all_items_in_menu(cls, FALSE)) {
        return NULL;
    }

    if (!parent_win_obj) {
        parent_window = CreateWindowEx(
            0, MESSAGE_WINDOW_CLASS_NAME, TEXT(""), WS_DISABLED, 
            0,0,0,0,NULL,NULL,NULL,NULL
        );
        if (!parent_window) {
            RAISE_LAST_ERROR();
            return NULL;
        }
    }

    
    BOOL result;
    Py_BEGIN_ALLOW_THREADS
    SetForegroundWindow(parent_window);

    // store parent window
    cls->parent_window = parent_window;

    // track menu
    result = TrackPopupMenuEx(cls->handle, flags, pos.x, pos.y, parent_window, NULL);

    // clear parent window
    cls->parent_window = NULL;

    PostMessage(parent_window, WM_NULL, 0, 0);
    Py_END_ALLOW_THREADS


    if (!parent_win_obj) {
        DestroyWindow(parent_window);
    }

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
        if (!PyObject_CallNoArgs(clicked_menu_item->string_check_data.callback)) {
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

PyObject*
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

PyObject*
menu_as_tuple(MenuTypeObject *cls, PyObject *arg) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    return PyList_AsTuple(cls->items_list);
}

static PyMethodDef menu_methods[] = {
    {"__init_subclass__", (PyCFunction)menu_init_subclass, METH_NOARGS|METH_CLASS, NULL},
    {"popup", (PyCFunction)menu_popup, METH_VARARGS|METH_KEYWORDS|METH_CLASS, NULL},
    {"close", (PyCFunction)menu_close, METH_NOARGS|METH_CLASS, NULL},
    {"as_tuple", (PyCFunction)menu_as_tuple, METH_NOARGS|METH_CLASS, NULL},
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
