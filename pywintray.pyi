import typing

def quit()->None:...
def mainloop()->None:...

MouseEventCallback:typing.TypeAlias = typing.Callable[[], typing.Any]

class TrayIcon:
    def __init__(
        self,
        icon_path: str|None = None,
        icon_handle: int|None = None,
        tip:str ="pywintray", 
        hidden:bool=False,
        load_icon_large: bool = False,
        load_icon_index: int = 0,
    )->None:...

    def show(self)->None:...
    def hide(self)->None:...
    def destroy(self)->None:...
    def update_icon(
        self,
        icon_path: str|None = None,
        icon_handle: int|None = None,
        load_icon_large: bool = False,
        load_icon_index: int = 0,
    )->None:...


    @property
    def tip(self)->str:...
    @tip.setter
    def tip(self, value:str)->None:...

    @property
    def hidden(self)->str:...

    @property
    def on_mouse_move(self) -> MouseEventCallback|None:...
    @on_mouse_move.setter
    def on_mouse_move(self, value:MouseEventCallback|None) -> None:...
