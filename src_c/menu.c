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

    if (cls->popup_event) {
        CloseHandle(cls->popup_event);
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

static PyObject *
menu_metaclass_new(PyTypeObject *meta, PyObject *args, PyObject *kwargs) {
    // inherit from the PyType_Type
    MenuTypeObject *cls = (MenuTypeObject *)PyType_Type.tp_new(meta, args, kwargs);
    if (!cls) {
        return NULL;
    }

    static char *kwlist[] = {"name", "bases", "namespace", NULL};

    PyObject *class_name, *class_bases, *class_namespace;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOO", kwlist,
        &class_name, &class_bases, &class_namespace
    )) {
        goto error_clean;
    }

    cls->items_list = NULL;
    cls->handle = NULL;
    cls->parent_window = NULL;
    cls->popup_event = NULL;

    // the class should not be subtyped any more
    ((PyTypeObject *)cls)->tp_flags &= ~(Py_TPFLAGS_BASETYPE);

    cls->items_list = PyList_New(0);
    if (cls->items_list==NULL) {
        goto error_clean;
    }

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(class_namespace, &pos, &key, &value)) {
        int is_menu_item = PyObject_IsInstance(value, (PyObject *)&MenuItemType);
        if (is_menu_item<0) {
            goto error_clean;
        }
        if (!is_menu_item) {
            continue;
        }
        MenuItemObject *menu_item = (MenuItemObject *)value;
        switch (menu_item->type) {
            case MENU_ITEM_TYPE_SEPARATOR:
            case MENU_ITEM_TYPE_STRING:
            case MENU_ITEM_TYPE_CHECK:
                break;
            case MENU_ITEM_TYPE_SUBMENU:
                if (!menu_item->sub) {
                    PyErr_SetString(PyExc_ValueError, "Invalid submenu");
                    goto error_clean;
                }
                break;
            default:
                PyErr_SetString(PyExc_SystemError, "Unknown menu item type");
                goto error_clean;
        }
        if (PyList_Append(cls->items_list, value)<0) {
            goto error_clean;
        }
    }

    cls->handle = CreatePopupMenu();
    if (cls->handle==NULL) {
        RAISE_LAST_ERROR();
        goto error_clean;
    }

    if(!update_all_items_in_menu(cls, TRUE)) {
        goto error_clean;
    }

    cls->popup_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!cls->popup_event) {
        RAISE_LAST_ERROR();
        goto error_clean;
    }

    return (PyObject *)cls;
error_clean:
    Py_XDECREF(cls);
    return NULL;
}

static BOOL
call_callback_by_id(UINT menu_item_id) {
    BOOL result = FALSE;
    PyGILState_STATE gstate = PyGILState_Ensure();
    idm_mutex_acquire(pwt_globals.menu_item_idm);

    MenuItemObject *clicked_menu_item = idm_get_data_by_id(pwt_globals.menu_item_idm, menu_item_id);
    if (!clicked_menu_item) {
        goto finally;
    }

    if (clicked_menu_item->type!=MENU_ITEM_TYPE_STRING &&
        clicked_menu_item->type!=MENU_ITEM_TYPE_CHECK) {
        PyErr_SetString(PyExc_SystemError, "This type of menu item doesn't have a callback");
        goto finally;
    }

    if (clicked_menu_item->callback) {
        if (!PyObject_CallOneArg(clicked_menu_item->callback, (PyObject *)clicked_menu_item)) {
            goto finally;
        }
    }

    result = TRUE;

finally:
    idm_mutex_release(pwt_globals.menu_item_idm);
    PyGILState_Release(gstate);
    return result;
}

static BOOL
update_target_menu_item(MenuTypeObject *menu, MenuItemObject *target_item) {
    for(Py_ssize_t i=0;i<PyList_GET_SIZE(menu->items_list);i++) {
        MenuItemObject *menu_item = (MenuItemObject *)PyList_GET_ITEM(menu->items_list, i);
        if (menu_item==target_item) {
            if (!update_menu_item(menu->handle, (UINT)i, menu_item, FALSE)) {
                return FALSE;
            }            
            continue;
        }
        if (menu_item->type==MENU_ITEM_TYPE_SUBMENU) {
            // recursive search and update in submenu
            if (!update_target_menu_item(menu_item->sub, target_item)) {
                return FALSE;
            }
        }
        
    }
    return TRUE;
}

static LRESULT CALLBACK
popup_menu_window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    switch (uMsg) {
        MenuTypeObject *menu;
        MenuItemObject *item;
        case WM_ENTERMENULOOP:
        case WM_EXITMENULOOP:
            menu = GetProp(hWnd, PYWINTRAY_MENU_OBJ_WINDOW_PROP_NAME);
            if (!menu) {
                break;
            }
            if (uMsg==WM_ENTERMENULOOP) {
                SetEvent(menu->popup_event);
            }
            else {
                ResetEvent(menu->popup_event);
            }
            break;
        case PYWINTRAY_MENU_UPDATE_MESSAGE:
            menu = GetProp(hWnd, PYWINTRAY_MENU_OBJ_WINDOW_PROP_NAME);
            if (!menu) {
                break;
            }
            item = (MenuItemObject *)lParam;
            if (!item) {
                break;
            }
        
            PyGILState_STATE gstate = PyGILState_Ensure();
            if (!update_target_menu_item(menu, item)) {
                PyErr_Print();
            }
            PyGILState_Release(gstate);
        default:
            break;
    }


    return DefWindowProc(hWnd, uMsg, wParam, lParam);
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
        *v_align_obj=NULL;
    
    BOOL allow_right_click = FALSE;

    POINT pos;

    UINT flags = 0;

    // Since there may not be message loop when Menu.popup is called,
    // and WM_COMMAND will be sent after TrackPopupMenu.
    // That means WM_COMMAND may never be received by window proc.
    // So we use TPM_RETURNCMD to receive the result.
    flags |= TPM_RETURNCMD;

    if(!PyArg_ParseTupleAndKeywords(
        args, kwargs, "|OpUU", kwlist,
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
    if (!h_align_obj || PyUnicode_EqualToUTF8(h_align_obj, "left")) {
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
    if (!v_align_obj || PyUnicode_EqualToUTF8(v_align_obj, "top")) {
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

    HWND parent_window = CreateWindowEx(
        0, MESSAGE_WINDOW_CLASS_NAME, TEXT(""), WS_DISABLED, 
        0,0,0,0,NULL,NULL,NULL,NULL
    );
    if (!parent_window) {
        RAISE_LAST_ERROR();
        return NULL;
    }

    // set window proc
    SetLastError(0);
    if (!SetWindowLongPtr(parent_window, GWLP_WNDPROC, (LONG_PTR)popup_menu_window_proc)) {
        DWORD err_code = GetLastError();
        if (err_code) {
            RAISE_WIN32_ERROR(err_code);
            goto clean_up_level_1;
        }
    }

    // set window prop
    if (!SetProp(parent_window, PYWINTRAY_MENU_OBJ_WINDOW_PROP_NAME, cls)) {
        RAISE_LAST_ERROR();
        goto clean_up_level_1;
    }
    
    // convert hwnd to u32
    UINT hwnd_u32 = (UINT)(intptr_t)parent_window;
    
    // add menu to active_menus
    if (!idm_put_id(pwt_globals.active_menus_idm, hwnd_u32, cls)) {
        goto clean_up_level_2;
    }
    
    SetForegroundWindow(parent_window);

    // store parent window
    cls->parent_window = parent_window;

    BOOL result;
    // track menu
    Py_BEGIN_ALLOW_THREADS
    result = TrackPopupMenuEx(cls->handle, flags, pos.x, pos.y, parent_window, NULL);
    Py_END_ALLOW_THREADS

    // clear parent window
    cls->parent_window = NULL;

    if (!idm_delete_id(pwt_globals.active_menus_idm, hwnd_u32)) {
        PyErr_Print();
    }
    
clean_up_level_2:
    RemoveProp(parent_window, PYWINTRAY_MENU_OBJ_WINDOW_PROP_NAME);
clean_up_level_1:
    DestroyWindow(parent_window);

    if (PyErr_Occurred()) {
        return NULL;
    }

    if (!result) {
        DWORD error_code = GetLastError();
        if (error_code) {
            RAISE_WIN32_ERROR(error_code);
            return NULL;
        }
    }
    else {
        if(!call_callback_by_id(result)) {
            return NULL;
        }
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

static BOOL
check_submenu_circular_reference(MenuTypeObject *tail, MenuTypeObject *head) {
    // FALSE: circular ref
    // TRUE: no ciecular ref 
    if (tail==head) {
        return FALSE;
    }
    for(Py_ssize_t i=0;i<PyList_GET_SIZE(head->items_list);i++) {
        MenuItemObject *menu_item = (MenuItemObject *)PyList_GET_ITEM(head->items_list, i);
        if (menu_item->type!=MENU_ITEM_TYPE_SUBMENU) {
            continue;
        }
        if (!check_submenu_circular_reference(tail, menu_item->sub)) {
            return FALSE;
        }
    }
    return TRUE;
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

    if (((MenuItemObject *)new_item)->type==MENU_ITEM_TYPE_SUBMENU) {
        MenuTypeObject *head=((MenuItemObject *)new_item)->sub;
        if (!check_submenu_circular_reference(cls, head)) {
            PyErr_SetString(PyExc_ValueError, "Circular submenu");
            return NULL;
        }
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

static PyObject*
menu_wait_for_popup(MenuTypeObject *cls, PyObject *args, PyObject* kwargs) {
    if(!menu_subtype_check((PyObject *)cls)) {
        return NULL;
    }

    static char *kwlist[] = {"timeout", NULL};

    double timeout = 0.0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|d", kwlist, &timeout)) {
        return NULL;
    }

    DWORD result;

    if (timeout<0.0) {
        result = WaitForSingleObject(cls->popup_event, 0);
    }
    else if (timeout==0.0) {
        result = WaitForSingleObject(cls->popup_event, INFINITE);
    }
    else {
        result = WaitForSingleObject(cls->popup_event, (DWORD)(timeout*1000.0));
    }
    

    if (result==WAIT_OBJECT_0) {
        Py_RETURN_TRUE;
    }
    else if (result==WAIT_TIMEOUT) {
        Py_RETURN_FALSE;
    }
    else {
        RAISE_LAST_ERROR();
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyMethodDef menu_methods[] = {
    {"popup", (PyCFunction)menu_popup, METH_VARARGS|METH_KEYWORDS|METH_CLASS, NULL},
    {"close", (PyCFunction)menu_close, METH_NOARGS|METH_CLASS, NULL},
    {"as_tuple", (PyCFunction)menu_as_tuple, METH_NOARGS|METH_CLASS, NULL},
    {"insert_item", (PyCFunction)menu_insert_item, METH_VARARGS|METH_CLASS, NULL},
    {"remove_item", (PyCFunction)menu_remove_item, METH_O|METH_CLASS, NULL},
    {"append_item", (PyCFunction)menu_append_item, METH_O|METH_CLASS, NULL},
    {"wait_for_popup", (PyCFunction)menu_wait_for_popup, METH_VARARGS|METH_KEYWORDS|METH_CLASS, NULL},
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

BOOL
init_menu_class(PyObject *module) {
    static PyType_Spec spec;

    PyType_Slot menu_metaclass_slots[] = {
        {Py_tp_setattr, menu_metaclass_setattr},
        {Py_tp_new, menu_metaclass_new},
        {Py_tp_dealloc, menu_metaclass_dealloc},
        {0, NULL}
    };

    spec.name = "pywintray.MenuMetaclass";
    spec.basicsize = sizeof(MenuTypeObject);
    spec.itemsize = 0;
    spec.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
    spec.slots = menu_metaclass_slots;

    PyObject *menu_metaclass = PyType_FromModuleAndSpec(
        module, &spec,
        (PyObject *)(&PyType_Type)
    );
    if(!menu_metaclass) {
        return FALSE;
    }

    ((PyObject *)(&MenuType))->ob_type = (PyTypeObject *)menu_metaclass;
    return TRUE;
}
