/*
This file implements the pywintray.MenuItem class
*/

#include "pywintray.h"

IDManager *menu_item_idm = NULL;

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
    UINT fMask, 

    // data that need allocation
    // or need extra checks
    WCHAR *string, 
    MenuItemUserData *user_data,
    HMENU submenu
) {
    info->cbSize = sizeof(MENUITEMINFO);
    info->fMask = fMask;
    info->fType = MFT_STRING;
    info->fState = 0;
    info->dwTypeData = NULL;
    if ((info->fMask)&MIIM_FTYPE){
        switch (menu_item->type) {
            case MENU_ITEM_TYPE_SEPARATOR:
                info->fType |= MFT_SEPARATOR;
                break;
            case MENU_ITEM_TYPE_STRING:
                break;
            case MENU_ITEM_TYPE_CHECK:
                if (menu_item->string_check_data.radio) {
                    info->fType |= MFT_RADIOCHECK;
                }
                info->fMask |= MIIM_STATE;
                break;
            case MENU_ITEM_TYPE_SUBMENU:
                info->fMask |= MIIM_SUBMENU;
                info->hSubMenu = submenu;
                break;
        }
    }
    if ((info->fMask)&MIIM_ID) {
        info->wID = menu_item->id;
    }
    if ((info->fMask)&MIIM_STATE) {
        if(!menu_item->enabled) {
            info->fState |= MFS_DISABLED;
        }
        if(menu_item->type==MENU_ITEM_TYPE_CHECK) {
            if (menu_item->string_check_data.checked) {
                info->fState |= MFS_CHECKED;
            }
        }
    }
    if ((info->fMask)&MIIM_STRING) {
        info->dwTypeData = string;
    }

    if ((info->fMask)&MIIM_DATA) {
        info->dwItemData = (ULONG_PTR)user_data;
    }
}

BOOL
update_menu_item(HMENU menu, UINT pos, MenuItemObject *obj, BOOL insert) {
    CHECK_MENU_ITEM_TYPE_VALID(obj, FALSE);

    WCHAR *string = NULL;
    BOOL result;
    MENUITEMINFO info;
    MenuItemUserData *user_data;

    // if in update mode, check the update counter
    // to ensure if the menu item needs update
    if (!insert) {
        info.fMask = MIIM_DATA|MIIM_ID;
        if (!GetMenuItemInfo(menu, pos, TRUE, &info)) {
            RAISE_LAST_ERROR();
            return FALSE;
        }
        if (info.wID!=obj->id) {
            PyErr_SetString(PyExc_SystemError, "Menu item id doesn't match");
            return FALSE;
        }
        user_data = (MenuItemUserData *)info.dwItemData;
        if (user_data->update_counter==obj->update_counter) {
            // the item is already up to date, return
            return TRUE;
        }
    }
    else { // if in insert mode, allocate the user data
        user_data = PWT_Malloc(sizeof(MenuItemUserData));
        if (!user_data) {
            PyErr_NoMemory();
            return FALSE;
        }
    }

    if (obj->type!=MENU_ITEM_TYPE_SEPARATOR) {
        string = PyUnicode_AsWideCharString(obj->string, NULL);
        if(!string) {
            goto error_clean;
        }
    }

    HMENU submenu_handle = NULL;
    if (obj->type==MENU_ITEM_TYPE_SUBMENU) {
        MenuTypeObject *submenu_obj = (MenuTypeObject *)(obj->submenu_data.sub);
        if(!submenu_obj) {
            PyErr_SetString(PyExc_SystemError, "Invalid submenu object");
            goto error_clean;
        }
        submenu_handle = submenu_obj->handle;
        if(!submenu_handle) {
            PyErr_SetString(PyExc_SystemError, "Invalid submenu handle");
            goto error_clean;
        }
    }

#define flags (MIIM_FTYPE|MIIM_ID|MIIM_STATE|MIIM_STRING|MIIM_DATA)
    build_menu_item_info_struct(obj, &info, flags, string, user_data, submenu_handle);
#undef flags
   
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
        goto error_clean;
    }

    // finally, update the update counter
    user_data->update_counter = obj->update_counter;

    return TRUE;

error_clean:
    if (insert && user_data) {
        PWT_Free(user_data);
    }
    if(string) {
        PyMem_Free(string);
    }
    return FALSE;
}

static MenuItemObject *
new_menu_item() {
    return (MenuItemObject *)MenuItemType.tp_alloc(&MenuItemType, 0);
}

static int
init_menu_item_generic(MenuItemObject *self) {
    self->id = idm_allocate_id(menu_item_idm, self);
    if(!self->id) {
        return -1;
    }

    self->type = MENU_ITEM_TYPE_NULL;
    self->update_counter = 0;
    self->string = NULL;
    self->enabled = TRUE;

    // data in the union is not initiallized here

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
    static char *kwlist[] = {"label", NULL};

    PyObject *string_obj = NULL;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U", kwlist, &string_obj)) {
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

    return (PyObject *)self;
}

static PyObject *
menu_item_check(PyObject *cls, PyObject *args, PyObject* kwargs) {
    static char *kwlist[] = {"label", "radio", "checked", NULL};

    PyObject *string_obj = NULL;

    BOOL checked = FALSE;
    BOOL radio = FALSE;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U|pp", kwlist,
        &string_obj,
        &radio,
        &checked
    )) {
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
    self->string_check_data.checked = checked;
    self->string_check_data.radio = radio;

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
    static char *kwlist[] = {"label", NULL};

    PyObject *string_obj = NULL;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "U", kwlist, &string_obj)) {
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

    if(!menu_subtype_check(arg)) {
        return NULL;
    }

    self->submenu_data.sub = arg;
    
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
        Py_DECREF(self->string_check_data.callback);
        self->string_check_data.callback = NULL;
        Py_RETURN_NONE;
    }

    if(!PyCallable_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Callback should be callable or None");
        return NULL;
    }

    self->string_check_data.callback = arg;

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

static PyObject *
menu_icon_get_sub(MenuItemObject *self, void *closure) {
    if(self->type!=MENU_ITEM_TYPE_SUBMENU) {
        PyErr_SetString(PyExc_TypeError, "This property is for submenu only");
        return NULL;
    }
    if(!self->submenu_data.sub) {
        PyErr_SetString(PyExc_TypeError, "Submenu is not registered");
        return NULL;
    }
    Py_INCREF(self->submenu_data.sub);
    return self->submenu_data.sub;
}

static PyGetSetDef menu_item_getset[] = {
    {"sub", (getter)menu_icon_get_sub, (setter)NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static void
menu_item_dealloc(MenuItemObject *self) {
    if (self->id) {
        idm_delete_id(menu_item_idm, self->id);
        self->id = 0;
    }
    Py_XDECREF(self->string);
    switch (self->id)
    {
        case MENU_ITEM_TYPE_STRING:
        case MENU_ITEM_TYPE_CHECK:
            Py_XDECREF(self->string_check_data.callback);
            break;
        case MENU_ITEM_TYPE_SUBMENU:
            Py_XDECREF(self->submenu_data.sub);
            break;
        default:
            break;
    }
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
