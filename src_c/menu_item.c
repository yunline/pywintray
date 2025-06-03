/*
This file implements the pywintray.MenuItem class
*/

#include "pywintray.h"

BOOL
check_menu_item_type_valid(MenuItemObject *menu_item) {
    if (
        (menu_item->type!=MENU_ITEM_TYPE_SEPARATOR) &&
        (menu_item->type!=MENU_ITEM_TYPE_STRING) &&
        (menu_item->type!=MENU_ITEM_TYPE_CHECK) &&
        (menu_item->type!=MENU_ITEM_TYPE_SUBMENU)
    ) {
        return FALSE;
    }
    return TRUE;
}

#define CHECK_MENU_ITEM_TYPE_VALID(mi, retv) \
    if (!check_menu_item_type_valid((mi))) { \
        PyErr_SetString(PyExc_SystemError, "Invalid menu item type"); \
        return (retv); \
    }


static void 
build_menu_item_info_struct(
    MenuItemObject *menu_item, 
    MENUITEMINFO *info, 

    // data that need allocation
    // or need extra checks
    WCHAR *string, 
    HMENU submenu
) {
    info->cbSize = sizeof(MENUITEMINFO);
    info->fMask = MIIM_FTYPE|MIIM_ID|MIIM_STATE|MIIM_DATA;
    info->fType = MFT_STRING;
    info->fState = 0;
    info->dwTypeData = NULL;

    switch (menu_item->type) {
        case MENU_ITEM_TYPE_SEPARATOR:
            info->fType |= MFT_SEPARATOR;
            break;
        case MENU_ITEM_TYPE_STRING:
            break;
        case MENU_ITEM_TYPE_CHECK:
            if (menu_item->radio) {
                info->fType |= MFT_RADIOCHECK;
            }
            break;
        case MENU_ITEM_TYPE_SUBMENU:
            info->fMask |= MIIM_SUBMENU;
            info->hSubMenu = submenu;
            break;
    }

    info->wID = menu_item->id;

    if(!menu_item->enabled && menu_item->type!=MENU_ITEM_TYPE_SEPARATOR) {
        info->fState |= MFS_DISABLED;
    }
    if(menu_item->type==MENU_ITEM_TYPE_CHECK) {
        if (menu_item->checked) {
            info->fState |= MFS_CHECKED;
        }
    }

    if (string) {
        info->fMask|=MIIM_STRING;
        info->dwTypeData = string;
    }

    // Use dwItemData to store the update counter
    info->dwItemData = (ULONG_PTR)menu_item->update_counter;
}

BOOL
update_menu_item(HMENU menu, UINT pos, MenuItemObject *menu_item, BOOL insert) {
    CHECK_MENU_ITEM_TYPE_VALID(menu_item, FALSE);

    WCHAR *string = NULL;
    BOOL result;
    MENUITEMINFO info;

    // if in update mode, check the update counter
    // to ensure if the menu item needs update
    // (except the submenu, which always need update)
    if (!insert && menu_item->type!=MENU_ITEM_TYPE_SUBMENU) {
        info.cbSize = sizeof(MENUITEMINFO);
        info.fMask = MIIM_DATA|MIIM_ID;
        if (!GetMenuItemInfo(menu, pos, TRUE, &info)) {
            RAISE_LAST_ERROR();
            return FALSE;
        }
        if (info.wID!=menu_item->id) {
            PyErr_SetString(PyExc_SystemError, "Menu item id doesn't match");
            return FALSE;
        }
        if (info.dwItemData==menu_item->update_counter) {
            // the item is already up to date, return
            return TRUE;
        }
    }

    HMENU submenu_handle = NULL;
    if (menu_item->type==MENU_ITEM_TYPE_SUBMENU) {
        // update submenu items
        if (!update_all_items_in_menu(menu_item->sub, FALSE)) {
            return FALSE;
        }
        submenu_handle = menu_item->sub->handle;
        if(!submenu_handle) {
            PyErr_SetString(PyExc_SystemError, "Invalid submenu handle");
            return FALSE;
        }
    }

    if (menu_item->type!=MENU_ITEM_TYPE_SEPARATOR) {
        string = PyUnicode_AsWideCharString(menu_item->string, NULL);
        if(!string) {
            return FALSE;
        }
    }

    build_menu_item_info_struct(menu_item, &info, string, submenu_handle);
   
    if (insert) {
        result = InsertMenuItem(menu, pos, TRUE, &info);
    }
    else {
        result = SetMenuItemInfo(menu, pos, TRUE, &info);
    }

    if (string) {
        PyMem_Free(string);
        string=NULL;
    }

    if(!result) {
        RAISE_LAST_ERROR();
        return FALSE;
    }

    return TRUE;
}

BOOL
update_all_items_in_menu(MenuTypeObject *cls, BOOL insert) {
    if (insert) {
        if (GetMenuItemCount(cls->handle)) {
            PyErr_SetString(PyExc_SystemError, "menu has non-zero items");
            return FALSE;
        }
    }
    else {
        if (GetMenuItemCount(cls->handle) != PyList_GET_SIZE(cls->items_list)) {
            PyErr_SetString(PyExc_SystemError, "menu size mismatch");
            return FALSE;
        }
    }
    for(Py_ssize_t i=0;i<PyList_GET_SIZE(cls->items_list);i++) {
        MenuItemObject *menu_item = (MenuItemObject *)PyList_GET_ITEM(cls->items_list, i);
        if(!update_menu_item(cls->handle, (UINT)i, menu_item, insert)) {
            return FALSE;
        }
    }
    return TRUE;
}

static MenuItemObject *
new_menu_item() {
    return (MenuItemObject *)MenuItemType.tp_alloc(&MenuItemType, 0);
}

static int
init_menu_item_generic(MenuItemObject *self) {
    self->id = idm_allocate_id(pwt_globals.menu_item_idm, self);
    if(!self->id) {
        return -1;
    }

    self->type = MENU_ITEM_TYPE_NULL;
    self->update_counter = 0;
    self->string = NULL;
    self->enabled = TRUE;
    self->callback = NULL;
    self->checked = FALSE;
    self->radio = FALSE;
    self->sub = NULL;

    return 0;
}

static PyObject *
menu_item_separator(PyObject *cls, PyObject *args) {
    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }
    if(init_menu_item_generic(self)<0) {
        Py_DECREF(self);
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_SEPARATOR;
    return (PyObject *)self;
}

static PyObject *
menu_item_string(PyObject *cls, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"label", "enabled", "callback", NULL};

    PyObject *string_obj = NULL;
    PyObject *callback_obj = Py_None;
    BOOL enabled = TRUE;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U|pO", kwlist, 
        &string_obj, &enabled, &callback_obj
    )) {
        return NULL;
    }

    if (!Py_IsNone(callback_obj) && !PyCallable_Check(callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback should be callable or None");
        return NULL;
    }

    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }
    if(init_menu_item_generic(self)<0) {
        Py_DECREF(self);
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_STRING;
    self->string = string_obj;
    Py_INCREF(string_obj);
    self->enabled = enabled;
    if (!Py_IsNone(callback_obj)) {
        self->callback = callback_obj;
        Py_INCREF(callback_obj);
    }

    return (PyObject *)self;
}

static PyObject *
menu_item_check(PyObject *cls, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"label", "radio", "checked", "enabled", "callback", NULL};

    PyObject *string_obj = NULL;
    PyObject *callback_obj = Py_None;

    BOOL checked = FALSE;
    BOOL radio = FALSE;
    BOOL enabled = TRUE;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U|pppO", kwlist,
        &string_obj,
        &radio,
        &checked,
        &enabled,
        &callback_obj
    )) {
        return NULL;
    }

    if (!Py_IsNone(callback_obj) && !PyCallable_Check(callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback should be callable or None");
        return NULL;
    }

    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }
    if(init_menu_item_generic(self)<0) {
        Py_DECREF(self);
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_CHECK;
    self->string = string_obj;
    Py_INCREF(string_obj);
    self->enabled = TRUE;
    self->checked = checked;
    self->radio = radio;
    if (!Py_IsNone(callback_obj)) {
        self->callback = callback_obj;
        Py_INCREF(callback_obj);
    }

    return (PyObject *)self;
}

static PyObject *menu_item_submenu_decorator(MenuItemObject* self, PyObject *arg);

static PyMethodDef submenu_decorator_method_def = {
    .ml_name = "_submenu_decorator",
    .ml_meth = (PyCFunction)menu_item_submenu_decorator,
    .ml_flags = METH_O,
    .ml_doc = NULL
};

static PyObject *
menu_item_sbumenu(PyObject *cls, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"label", "enabled", NULL};

    PyObject *string_obj = NULL;
    BOOL enabled = TRUE;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U|p", kwlist, &string_obj, &enabled)) {
        return NULL;
    }

    MenuItemObject *self = new_menu_item();
    if(!self) {
        return NULL;
    }
    if(init_menu_item_generic(self)<0) {
        Py_DECREF(self);
        return NULL;
    }

    self->type = MENU_ITEM_TYPE_SUBMENU;
    self->string = string_obj;
    Py_INCREF(string_obj);
    self->enabled = enabled;

    PyObject *decorator = PyCFunction_New(&submenu_decorator_method_def, (PyObject *)self);
    
    Py_DECREF(self);

    return decorator;
}

static PyObject *
menu_item_submenu_decorator(MenuItemObject* self, PyObject *arg) {
    if(self->type!=MENU_ITEM_TYPE_SUBMENU) {
        PyErr_SetString(PyExc_TypeError, "This method is for submenu only");
        return NULL;
    }

    if (self->sub) {
        PyErr_SetString(PyExc_RuntimeError, "You can only set the submenu once");
        return NULL;
    }

    if(!menu_subtype_check(arg)) {
        return NULL;
    }

    self->sub = (MenuTypeObject *)arg;
    
    Py_INCREF(arg);
    Py_INCREF(self);

    return (PyObject *)self;
}

static PyObject *
menu_item_register_callback(MenuItemObject* self, PyObject *arg) {
    if (
        (self->type!=MENU_ITEM_TYPE_STRING) &&
        (self->type!=MENU_ITEM_TYPE_CHECK)
    ) {
        PyErr_SetString(PyExc_TypeError, "This menu item doesn't support callback");
        return NULL;
    }

    if (Py_IsNone(arg)) {
        Py_XDECREF(self->callback);
        self->callback = NULL;
        Py_RETURN_NONE;
    }

    if(!PyCallable_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Callback should be callable or None");
        return NULL;
    }

    self->callback = arg;

    // store the object to self->callback, incref
    Py_INCREF(arg);

    // return the object, incref
    Py_INCREF(arg);

    return arg;
}

static PyMethodDef menu_item_methods[] = {
    // class methods
    {"separator", (PyCFunction)menu_item_separator, METH_NOARGS|METH_CLASS, NULL},
    {"string", (PyCFunction)menu_item_string, METH_VARARGS|METH_KEYWORDS|METH_CLASS, NULL},
    {"check", (PyCFunction)menu_item_check, METH_VARARGS|METH_KEYWORDS|METH_CLASS, NULL},
    {"submenu", (PyCFunction)menu_item_sbumenu, METH_VARARGS|METH_KEYWORDS|METH_CLASS, NULL},
    // methods
    {"register_callback", (PyCFunction)menu_item_register_callback, METH_O, NULL},
    {NULL, NULL, 0, NULL}
};

static void
post_update_message(MenuItemObject *menu_item) {
    UINT hwnd_u32;
    Py_ssize_t pos = 0;

    idm_mutex_acquire(pwt_globals.active_menus_idm);
    while (idm_next(pwt_globals.active_menus_idm, &pos, &hwnd_u32, NULL)) {
        if (hwnd_u32==((UINT)-1) && PyErr_Occurred()) {
            PyErr_Print();
            continue;
        }
        HWND hwnd = (HWND)(intptr_t)hwnd_u32;
        PostMessage(hwnd, PYWINTRAY_MENU_UPDATE_MESSAGE, 0, (LPARAM)menu_item);
    }
    idm_mutex_release(pwt_globals.active_menus_idm);
}

static PyObject *
menu_item_get_sub(MenuItemObject *self, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_SUBMENU) {
        PyErr_SetString(PyExc_TypeError, "This property is for submenu only");
        return NULL;
    }
    if(!self->sub) {
        PyErr_SetString(PyExc_TypeError, "Submenu is not registered");
        return NULL;
    }
    Py_INCREF(self->sub);
    return (PyObject *)self->sub;
}

static PyObject *
menu_item_get_label(MenuItemObject *self, void *closure) {
    if(self->type==MENU_ITEM_TYPE_SEPARATOR) {
        PyErr_SetString(PyExc_TypeError, "Separator doesn't support label property");
        return NULL;
    }
    Py_INCREF(self->string);
    return self->string;
}

static int
menu_item_set_label(MenuItemObject *self, PyObject *value, void *closure) {
    if(self->type==MENU_ITEM_TYPE_SEPARATOR) {
        PyErr_SetString(PyExc_TypeError, "Separator doesn't support label property");
        return -1;
    }
    if(!PyUnicode_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Type of 'label' must be str");
        return -1;
    }
    Py_DECREF(self->string);
    self->string = value;
    Py_INCREF(self->string);
    self->update_counter++;
    post_update_message(self);
    return 0;
}

static PyObject *
menu_item_get_checked(MenuItemObject *self, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_TypeError, "This property is for check only");
        return NULL;
    }
    if (self->checked) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static int
menu_item_set_checked(MenuItemObject *self, PyObject *value, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_TypeError, "This property is for check only");
        return -1;
    }
    int result = PyObject_IsTrue(value);
    if(result<0) {
        return -1;
    }
    self->checked = result;
    self->update_counter++;
    post_update_message(self);
    return 0;
}

static PyObject *
menu_item_get_radio(MenuItemObject *self, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_TypeError, "This property is for check only");
        return NULL;
    }
    if (self->radio) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static int
menu_item_set_radio(MenuItemObject *self, PyObject *value, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_TypeError, "This property is for check only");
        return -1;
    }
    int result = PyObject_IsTrue(value);
    if(result<0) {
        return -1;
    }
    self->radio = result;
    self->update_counter++;
    post_update_message(self);
    return 0;
}

static PyObject *
menu_item_get_enabled(MenuItemObject *self, void *closure) {
    if(self->type==MENU_ITEM_TYPE_SEPARATOR) {
        PyErr_SetString(PyExc_TypeError, "Separator doesn't support this property");
        return NULL;
    }
    if (self->enabled) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static int
menu_item_set_enabled(MenuItemObject *self, PyObject *value, void *closure) {
    if(self->type==MENU_ITEM_TYPE_SEPARATOR) {
        PyErr_SetString(PyExc_TypeError, "Separator doesn't support this property");
        return -1;
    }
    int result = PyObject_IsTrue(value);
    if(result<0) {
        return -1;
    }
    self->enabled = result;
    self->update_counter++;
    post_update_message(self);
    return 0;
}

static PyObject *
menu_item_get_type(MenuItemObject *self, void *closure) {
    const char* type_string;
    switch (self->type) {
        case MENU_ITEM_TYPE_SEPARATOR:
            type_string="separator";
            break;
        case MENU_ITEM_TYPE_STRING:
            type_string="string";
            break;
        case MENU_ITEM_TYPE_CHECK:
            type_string="check";
            break;
        case MENU_ITEM_TYPE_SUBMENU:
            type_string="submenu";
            break;
        default:
            PyErr_Format(PyExc_SystemError, "Unknown menu item type %d", self->type);
            return NULL;
    }
    return PyUnicode_FromString(type_string);
}

static PyGetSetDef menu_item_getset[] = {
    {"sub", (getter)menu_item_get_sub, (setter)NULL, NULL, NULL},
    {"label", (getter)menu_item_get_label, (setter)menu_item_set_label, NULL, NULL},
    {"checked", (getter)menu_item_get_checked, (setter)menu_item_set_checked, NULL, NULL},
    {"radio", (getter)menu_item_get_radio, (setter)menu_item_set_radio, NULL, NULL},
    {"enabled", (getter)menu_item_get_enabled, (setter)menu_item_set_enabled, NULL, NULL},
    {"type", (getter)menu_item_get_type, (setter)NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static void
menu_item_dealloc(MenuItemObject *self) {
    if (self->id) {
        idm_delete_id(pwt_globals.menu_item_idm, self->id);
        self->id = 0;
    }
    Py_XDECREF(self->string);
    Py_XDECREF(self->callback);
    Py_XDECREF(self->sub);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject*
menu_item_repr(MenuItemObject *self) {
    const char *type_string;
    switch (self->type)
    {
        case MENU_ITEM_TYPE_NULL:
            type_string="null";
            goto end_no_arg;
        case MENU_ITEM_TYPE_SEPARATOR:
            type_string="separator";
            goto end_no_arg;
        case MENU_ITEM_TYPE_STRING:
            type_string="string";
            goto end_string;
        case MENU_ITEM_TYPE_CHECK:
            type_string="check";
            goto end_string;
        case MENU_ITEM_TYPE_SUBMENU:
            type_string="submenu";
            goto end_string;
        default:
            PyErr_Format(PyExc_SystemError, "Unknown menu item type %d", self->type);
            return NULL;
    }

end_no_arg:
    return PyUnicode_FromFormat("<MenuItem(type=%s)>", type_string);
end_string:
    return PyUnicode_FromFormat("<MenuItem(type=%s, string=%R)>", type_string, self->string);
}

PyTypeObject MenuItemType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pywintray.MenuItem",
    .tp_doc = NULL,
    .tp_basicsize = sizeof(MenuItemObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)menu_item_dealloc,
    .tp_repr = (reprfunc)menu_item_repr,
    .tp_methods = menu_item_methods,
    .tp_getset = menu_item_getset
};
