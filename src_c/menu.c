/*
This file implements the pywintray.Menu class
*/

#include "pywintray.h"

// Caller must hold `menu_insert_delete_cs` critical section
static BOOL
update_menu_item(HMENU menu, UINT pos, MenuItemObject *menu_item, BOOL insert);
// Caller must hold `menu_insert_delete_cs` critical section
static BOOL
update_all_items_in_menu(MenuTypeObject *menu, BOOL insert);
// Caller must hold `menu_insert_delete_cs` critical section
static BOOL
update_target_item_in_menu(MenuTypeObject *menu, MenuItemObject *target_item);


static BOOL
update_menu_item(HMENU menu, UINT pos, MenuItemObject *menu_item, BOOL insert) {
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

    info.cbSize = sizeof(MENUITEMINFO);
    info.fMask = MIIM_FTYPE|MIIM_ID|MIIM_STATE|MIIM_DATA;
    info.fType = MFT_STRING;
    info.fState = 0;
    info.dwTypeData = NULL;

    switch (menu_item->type) {
        case MENU_ITEM_TYPE_SEPARATOR:
            info.fType |= MFT_SEPARATOR;
            break;
        case MENU_ITEM_TYPE_STRING:
            break;
        case MENU_ITEM_TYPE_CHECK:
            if (menu_item->radio) {
                info.fType |= MFT_RADIOCHECK;
            }
            break;
        case MENU_ITEM_TYPE_SUBMENU:
            info.fMask |= MIIM_SUBMENU;
            info.hSubMenu = submenu_handle;
            break;
    }

    info.wID = menu_item->id;

    if(!menu_item->enabled && menu_item->type!=MENU_ITEM_TYPE_SEPARATOR) {
        info.fState |= MFS_DISABLED;
    }
    if(menu_item->type==MENU_ITEM_TYPE_CHECK) {
        if (menu_item->checked) {
            info.fState |= MFS_CHECKED;
        }
    }

    if (string) {
        info.fMask|=MIIM_STRING;
        info.dwTypeData = string;
    }

    // Use dwItemData to store the update counter
    info.dwItemData = (ULONG_PTR)menu_item->update_counter;

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

static BOOL
update_all_items_in_menu(MenuTypeObject *menu, BOOL insert) {
    if (insert) {
        if (GetMenuItemCount(menu->handle)) {
            PyErr_SetString(PyExc_SystemError, "menu has non-zero items");
            return FALSE;
        }
    }
    else {
        if (GetMenuItemCount(menu->handle) != PyList_GET_SIZE(menu->items_list)) {
            PyErr_SetString(PyExc_SystemError, "menu size mismatch");
            return FALSE;
        }
    }
    for(Py_ssize_t i=0;i<PyList_GET_SIZE(menu->items_list);i++) {
        MenuItemObject *menu_item = (MenuItemObject *)PyList_GET_ITEM(menu->items_list, i);
        if(!update_menu_item(menu->handle, (UINT)i, menu_item, insert)) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL
update_target_item_in_menu(MenuTypeObject *menu, MenuItemObject *target_item) {
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
            if (!update_target_item_in_menu(menu_item->sub, target_item)) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

static int
menu_metaclass_setattr(MenuTypeObject *self, char *attr, PyObject *value) {
    PyErr_SetString(PyExc_AttributeError, "This class doesn't support setting attribute");
    return -1;
}

static void
menu_metaclass_dealloc(MenuTypeObject *cls) {
    if (cls==pwt_globals.MenuType) {
        // The size of subtypes of Menu is the size of MenuTypeObject.
        // However the size of Menu itself is the size of PyHeapTypeObject.
        // 
        // So when deallocating Menu itself,
        // let's replace its ob_type back to PyType_Type, then deallocate and return

        PyObject *cls_obj = ((PyObject *)cls);

        Py_DECREF(cls_obj->ob_type);
        cls_obj->ob_type = (&PyType_Type);
        Py_INCREF(cls_obj->ob_type);

        PyType_Type.tp_dealloc(cls_obj);

        return;

        // If the PyType_FromMetaclass (3.12+) is used
        // then all these above can be removed
    }

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
        return FALSE;
    }
    PyTypeObject *cls = (PyTypeObject *)pwt_globals.MenuType;
    if(
        !PyType_IsSubtype((PyTypeObject *)arg, cls) ||
        arg==(PyObject *)(cls)
    ) {
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
        int is_menu_item = PyObject_IsInstance(value, (PyObject *)(pwt_globals.MenuItemType));
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

    PWT_ENTER_MENU_INSERT_DELETE_CS();
    BOOL update_result = update_all_items_in_menu(cls, TRUE);
    PWT_LEAVE_MENU_INSERT_DELETE_CS();
    if(!update_result) {
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
    idm_enter_critical_section(pwt_globals.menu_item_idm);

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
    idm_leave_critical_section(pwt_globals.menu_item_idm);
    PyGILState_Release(gstate);
    return result;
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
            PWT_ENTER_MENU_INSERT_DELETE_CS();
            if (!update_target_item_in_menu(menu, item)) {
                PyErr_Print();
            }
            PWT_LEAVE_MENU_INSERT_DELETE_CS();
            PyGILState_Release(gstate);
            break;
        default:
            break;
    }


    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#define CHECK_MENU_SUBTYPE(o, r) {\
    if (!menu_subtype_check((PyObject *)(o))) { \
        PyErr_SetString(PyExc_TypeError, "Argument 'cls' must be subtype of Menu"); \
        return (r); \
    } \
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

    CHECK_MENU_SUBTYPE(cls, NULL);

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
    PWT_ENTER_MENU_INSERT_DELETE_CS();
    BOOL update_result = update_all_items_in_menu(cls, FALSE);
    PWT_LEAVE_MENU_INSERT_DELETE_CS();
    if (!update_result) {
        return NULL;
    }

    HWND parent_window = CreateWindowEx(
        0, PWT_WINDOW_CLASS_NAME, TEXT(""), WS_DISABLED, 
        0, 0, 0, 0, NULL, NULL, pwt_dll_hinstance, NULL
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
    CHECK_MENU_SUBTYPE(cls, NULL);

    if(!cls->parent_window) {
        Py_RETURN_NONE;
    }
    PostMessage(cls->parent_window, WM_CANCELMODE, 0, 0);
    Py_RETURN_NONE;
}

static PyObject*
menu_as_tuple(MenuTypeObject *cls, PyObject *arg) {
    CHECK_MENU_SUBTYPE(cls, NULL);

    PWT_ENTER_MENU_INSERT_DELETE_CS();
    PyObject *result = PyList_AsTuple(cls->items_list);
    PWT_LEAVE_MENU_INSERT_DELETE_CS();

    return result;
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
    CHECK_MENU_SUBTYPE(cls, NULL);

    PyObject *new_item;
    Py_ssize_t index;
    if (!PyArg_ParseTuple(args, "nO!", &index, pwt_globals.MenuItemType, &new_item)) {
        return NULL;
    }

    PWT_ENTER_MENU_INSERT_DELETE_CS();

    if (((MenuItemObject *)new_item)->type==MENU_ITEM_TYPE_SUBMENU) {
        MenuTypeObject *head=((MenuItemObject *)new_item)->sub;
        if (!check_submenu_circular_reference(cls, head)) {
            PyErr_SetString(PyExc_ValueError, "Circular submenu");
            goto error_clean;
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
        goto error_clean;
    }

    if(!update_menu_item(cls->handle, (UINT)index, (MenuItemObject *)new_item, TRUE)) {
        PySequence_DelItem(cls->items_list, index);
        goto error_clean;
    }

    PWT_LEAVE_MENU_INSERT_DELETE_CS();
    Py_RETURN_NONE;

error_clean:
    PWT_LEAVE_MENU_INSERT_DELETE_CS();
    return NULL;
}

static PyObject *
menu_remove_item(MenuTypeObject *cls, PyObject *arg) {
    CHECK_MENU_SUBTYPE(cls, NULL);

    if (!PyLong_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Argument mus be an int");
        return NULL;
    }
    Py_ssize_t index = PyLong_AsSsize_t(arg);
    if (index==-1 && PyErr_Occurred()) {
        return NULL;
    }

    PWT_ENTER_MENU_INSERT_DELETE_CS();

    Py_ssize_t list_size = PyList_GET_SIZE(cls->items_list);

    // normalize index
    if (index<0) {
        index += list_size;
    }
    if (index<0 || index>=list_size) {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        goto error_clean;
    }

    if (!RemoveMenu(cls->handle, (UINT)index, MF_BYPOSITION)) {
        RAISE_LAST_ERROR();
        goto error_clean;
    }

    if (PySequence_DelItem(cls->items_list, index)<0) {
        PyErr_SetString(PyExc_SystemError, "Unable to delete item from internal list");
        goto error_clean;
    }

    PWT_LEAVE_MENU_INSERT_DELETE_CS();
    Py_RETURN_NONE;

error_clean:
    PWT_LEAVE_MENU_INSERT_DELETE_CS();
    return NULL;
}

static PyObject *
menu_append_item(MenuTypeObject *cls, PyObject *arg) {
    CHECK_MENU_SUBTYPE(cls, NULL);

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
    CHECK_MENU_SUBTYPE(cls, NULL);

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

MenuTypeObject *
create_menu_type(PyObject *module) {
    static PyType_Spec menu_metaclass_spec;

    PyType_Slot menu_metaclass_slots[] = {
        {Py_tp_setattr, menu_metaclass_setattr},
        {Py_tp_new, menu_metaclass_new},
        {Py_tp_dealloc, menu_metaclass_dealloc},
        {0, NULL}
    };

    menu_metaclass_spec.name = "pywintray._MenuMetaclass";
    menu_metaclass_spec.basicsize = sizeof(MenuTypeObject);
    menu_metaclass_spec.itemsize = 0;
    menu_metaclass_spec.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE;
    menu_metaclass_spec.slots = menu_metaclass_slots;

    PyObject *menu_metaclass = PyType_FromModuleAndSpec(
        module, &menu_metaclass_spec,
        (PyObject *)(&PyType_Type)
    );
    if(!menu_metaclass) {
        return NULL;
    }

    static PyType_Spec menu_spec;

    PyType_Slot menu_slots[] = {
        {Py_tp_methods, menu_methods},
        {0, NULL}
    };

    menu_spec.name = "pywintray.Menu";
    menu_spec.basicsize = sizeof(PyObject);
    menu_spec.itemsize = 0;
    menu_spec.flags = 
        Py_TPFLAGS_DEFAULT | 
        Py_TPFLAGS_BASETYPE |
        Py_TPFLAGS_DISALLOW_INSTANTIATION |
        Py_TPFLAGS_IMMUTABLETYPE;
    menu_spec.slots = menu_slots;

    PyObject *menu_class = PyType_FromModuleAndSpec(module, &menu_spec, NULL);
    // For metaclass cases, PyType_FromMetaclass should be used.
    // PyType_FromMetaclass is 3.12+ only. 
    // However we need to keep 3.10+ compat.
    // That is why PyType_FromModuleAndSpec is used
    // and the ob_type of the class object is replaced.
    // Fortunately, this solves the problem.

    if (menu_class) {
        // replace the metaclass
        Py_DECREF(menu_class->ob_type);
        menu_class->ob_type = (PyTypeObject *)menu_metaclass;
        Py_INCREF(menu_class->ob_type);
    }

    Py_DECREF(menu_metaclass);

    return (MenuTypeObject *)menu_class;
}
