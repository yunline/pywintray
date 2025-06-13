import typing

@typing.final
class IconHandle:
    def __new__(cls, value:int)->IconHandle:...

def load_icon(filename:str, large:bool=True, index:int=0)->IconHandle:...

_TrayIconCallback: typing.TypeAlias = typing.Callable[[TrayIcon], typing.Any]

_TrayIconCallbackTypes: typing.TypeAlias = typing.Literal[
    'mouse_move', 
    'mouse_left_button_down', 
    'mouse_left_button_up', 
    'mouse_left_double_click',
    'mouse_right_button_down', 
    'mouse_right_button_up', 
    'mouse_right_double_click',
    'mouse_mid_button_down', 
    'mouse_mid_button_up', 
    'mouse_mid_double_click',
    'notification_click',
    'notification_timeout'
]

@typing.final
class TrayIcon:
    def __new__(
        cls,
        icon_handle: IconHandle,
        tip:str ="pywintray", 
        hidden:bool=False,
    )->TrayIcon:...

    def show(self)->None:...
    def hide(self)->None:...

    @typing.overload
    def register_callback(
        self, 
        callback_type:_TrayIconCallbackTypes, 
        callback:_TrayIconCallback|None
    ) -> None:...

    @typing.overload
    def register_callback(self, callback_type:_TrayIconCallbackTypes) \
        -> typing.Callable[[_TrayIconCallback], _TrayIconCallback]:...
    
    def notify(
        self, 
        title:str, 
        message:str, 
        no_sound:bool = False,
        icon:IconHandle|None = None
    ) -> None:...

    @property
    def tip(self)->str:...
    @tip.setter
    def tip(self, value:str)->None:...

    @property
    def hidden(self)->bool:...
    @hidden.setter
    def hidden(self, value: bool)->None:...

    @property
    def icon_handle(self)->IconHandle:...
    @icon_handle.setter
    def icon_handle(self, value:IconHandle)->None:...

def start_tray_loop()->None:...
def stop_tray_loop()->None:...
def wait_for_tray_loop_ready(timeout:float=0.0)->bool:...

class _MenuMetaclass(type):
    pass

class Menu(metaclass=_MenuMetaclass):
    @classmethod
    def popup(
        cls,
        position: tuple[int, int]|None=None,
        allow_right_click: bool=False,
        horizontal_align: typing.Literal["left", "center", "right"]="left",
        vertical_align: typing.Literal["top", "center", "bottom"]="top",
    )->None:...
    @classmethod
    def wait_for_popup(cls, timeout:float=0.0)->bool:...
    @classmethod
    def close(cls)->None:...
    @classmethod
    def as_tuple(cls)->tuple[MenuItem, ...]:...
    @classmethod
    def insert_item(cls, index:int, item:MenuItem) -> None:...
    @classmethod
    def append_item(cls, item:MenuItem)->None:...
    @classmethod
    def remove_item(cls, index:int) -> None:...


_MenuItemCallback: typing.TypeAlias = typing.Callable[[MenuItem], typing.Any]

_Separator: typing.TypeAlias = typing.Literal["separator"]
_String: typing.TypeAlias = typing.Literal["string"]
_Check: typing.TypeAlias = typing.Literal["check"]
_Submenu: typing.TypeAlias = typing.Literal["submenu"]

_MENU_TYPE = typing.TypeVar(
    "_MENU_TYPE", 
    _Separator,
    _String,
    _Check,
    _Submenu,
)

_SUBMENU = typing.TypeVar(
    "_SUBMENU",
    bound=Menu|None,
    default=None
)

@typing.final
class _MenuItemMetaclass(type):
    def separator(cls)->MenuItem[_Separator]:...

    def string(
        cls, 
        label:str, 
        enabled:bool=True, 
        callback:_MenuItemCallback|None=None
    )->MenuItem[_String]:...

    def check(
        cls, 
        label:str, 
        radio:bool=False, 
        checked:bool=False, 
        enabled:bool=True,
        callback:_MenuItemCallback|None=None
    )->MenuItem[_Check]:...

    def submenu(
        cls, 
        label:str, 
        enabled:bool=True
    )->typing.Callable[[type[_SUBMENU]], MenuItem[_Submenu, _SUBMENU]]:...


@typing.final
class MenuItem(typing.Generic[_MENU_TYPE, _SUBMENU], metaclass=_MenuItemMetaclass):
    def register_callback(
        self:MenuItem[_String]|MenuItem[_Check], 
        fn:_MenuItemCallback
    ) -> _MenuItemCallback:...

    @property
    def sub(self:MenuItem[_Submenu, _SUBMENU])->type[_SUBMENU]:...

    @property
    def label(self:MenuItem[_String]|MenuItem[_Check]|MenuItem[_Submenu]) -> str:...
    @label.setter
    def label(self:MenuItem[_String]|MenuItem[_Check]|MenuItem[_Submenu], value:str) -> None:...

    @property
    def checked(self:MenuItem[_Check]) -> bool:...
    @checked.setter
    def checked(self:MenuItem[_Check], value:bool) -> None:...

    @property
    def radio(self:MenuItem[_Check]) -> bool:...
    @radio.setter
    def radio(self:MenuItem[_Check], value:bool) -> None:...

    @property
    def enabled(self:MenuItem[_String]|MenuItem[_Check]|MenuItem[_Submenu]) -> bool:...
    @enabled.setter
    def enabled(self:MenuItem[_String]|MenuItem[_Check]|MenuItem[_Submenu], value:bool) -> None:...

    @property
    def type(self) -> _MENU_TYPE:...

__version__:str
VERSION: tuple[int, int, int]
