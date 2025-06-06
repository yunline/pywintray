import typing
import pywintray

def get_internal_tray_icon_dict() -> typing.Mapping[int, typing.Any]:...
def get_internal_menu_item_dict() -> typing.Mapping[int, typing.Any]:...

def get_internal_id(
    obj: \
        pywintray.IconHandle |
        pywintray.MenuItem |
        pywintray.TrayIcon |
        type[pywintray.Menu]
) -> int:...
