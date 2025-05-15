import typing

class IconHandle:
    @classmethod
    def from_int(cls, value:int)->IconHandle:...
    @property
    def value(self)->int:...

def quit()->None:...
def mainloop()->None:...
def load_icon(filename:str, large:bool=False, index:int=0)->IconHandle:...

_TrayIconCallback: typing.TypeAlias = typing.Callable[["TrayIcon"], typing.Any]

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
]
class TrayIcon:
    def __init__(
        self,
        icon_handle: IconHandle,
        tip:str ="pywintray", 
        hidden:bool=False,
    )->None:...

    def show(self)->None:...
    def hide(self)->None:...
    def destroy(self)->None:...
    def update_icon(self, icon_handle: IconHandle)->None:...

    @typing.overload
    def register_callback(
        self, 
        callback_type:_TrayIconCallbackTypes, 
        callback:_TrayIconCallback|None
    ) -> None:...

    @typing.overload
    def register_callback(self, callback_type:_TrayIconCallbackTypes) \
        -> typing.Callable[[_TrayIconCallback], _TrayIconCallback]:...

    @property
    def tip(self)->str:...
    @tip.setter
    def tip(self, value:str)->None:...

    @property
    def hidden(self)->str:...

class Menu:
    @classmethod
    def popup(
        cls,
        position: tuple[int, int]|None=None,
        allow_right_click: bool=False,
        horizontal_align: typing.Literal["left", "center", "right"]="left",
        vertical_align: typing.Literal["top", "center", "bottom"]="top",
        parent_window: int|None = None,
    )->None:...
    @classmethod
    def close(cls)->None:...
    @classmethod
    def as_tuple(cls)->tuple["MenuItem", ...]:...
    @classmethod
    def insert_item(cls, index:int, item:"MenuItem") -> None:...
    @classmethod
    def append_item(cls, item:"MenuItem")->None:...
    @classmethod
    def remove_item(cls, index:int) -> None:...

    @property
    def poped_up(cls) -> bool:...

MenuItemCallback: typing.TypeAlias = typing.Callable[["MenuItem"], typing.Any]

Separator: typing.TypeAlias = typing.Literal["separator"]
String: typing.TypeAlias = typing.Literal["string"]
Check: typing.TypeAlias = typing.Literal["check"]
Submenu: typing.TypeAlias = typing.Literal["submenu"]

T = typing.TypeVar(
    "T", 
    Separator,
    String,
    Check,
    Submenu,
)

@typing.final
class MenuItem(typing.Generic[T]):
    @classmethod
    def separator(cls)->MenuItem[Separator]:...
    @classmethod
    def string(cls, label:str, enabled:bool=True)->MenuItem[String]:...
    @classmethod
    def check(cls, label:str, radio:bool=False, checked:bool=False, enabled:bool=True)->MenuItem[Check]:...
    @classmethod
    def submenu(cls, label:str, enabled:bool=True)->typing.Callable[[type[Menu]], MenuItem[Submenu]]:...

    def register_callback(self:MenuItem[String]|MenuItem[Check], fn:MenuItemCallback) -> MenuItemCallback:...

    @property
    def sub(self:MenuItem[Submenu])->type[Menu]:...

    @property
    def label(self:MenuItem[String]|MenuItem[Check]|MenuItem[Submenu]) -> str:...
    @label.setter
    def label(self:MenuItem[String]|MenuItem[Check]|MenuItem[Submenu], value:str) -> None:...

    @property
    def checked(self:MenuItem[Check]) -> bool:...
    @checked.setter
    def checked(self:MenuItem[Check], value:bool) -> None:...

    @property
    def radio(self:MenuItem[Check]) -> bool:...
    @radio.setter
    def radio(self:MenuItem[Check], value:bool) -> None:...

    @property
    def enabled(self:MenuItem[String]|MenuItem[Check]|MenuItem[Submenu]) -> bool:...
    @enabled.setter
    def enabled(self:MenuItem[String]|MenuItem[Check]|MenuItem[Submenu], value:bool) -> None:...

    @property
    def type(self) -> T:...

__version__:str
VERSION: tuple[int, int, int]
