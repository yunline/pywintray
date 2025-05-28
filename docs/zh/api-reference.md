# API 参考

{{ API("load_icon", "从 `.ico`、`.dll` 或 `.exe` 文件中加载图标") }}

**参数**  

|参数|注解|
|-|-|
|`filename`|要加载的图标文件的路径|
|`large`|是否加载大图标。设为 `True`（默认）则加载大图标（分辨率一般是32x32，取决于DPI设置），设为 `False` 则加载小图标（分辨率一般是16x16）|
|`index`|加载的文件可能包含多个图标，本参数指定要加载的图标的序号。默认值为0|

**返回值**  

返回一个 {{ REF("IconHandle") }} 对象。

{{ API("IconHandle", "图标类型") }}

**备注**  

本类型不能直接实例化。要获取IconHandle的实例，
请使用 {{ REF("load_icon") }} 函数或 {{ REF("IconHandle.from_int") }} 方法。

{{ API("IconHandle.from_int", "将用户提供的整数句柄转换成IconHandle") }}

!!! warning

    本方法如果使用不当，可能会造成内存泄漏等未知错误。
    如果只是想从文件里加载图标，请用 {{ REF("load_icon") }}。

**参数**  

|参数|注解|
|-|-|
|`value`|将要转换成 {{ REF("IconHandle") }} 的整数图标句柄|

**返回值**

一个 {{ REF("IconHandle") }} 实例。

**备注**  

本方法会检查传入值是否为 `0`，如果为 `0` 则抛出 `#!python ValueError`。
除此之外本方法不会对传入的句柄做任何检查，请确保传入的句柄有效。

由于句柄由用户提供，{{ REF("IconHandle") }} 不可能知道句柄需要在何时被回收。
所以由本方法返回的 {{ REF("IconHandle") }} 实例并不会自动回收用户提供的句柄。
用户需要自己负责管理句柄的回收。

以下是一个例子
```py
# 获得一个图标句柄
hicon = ctypes.windll.shell32.ExtractIconW(
    ctypes.windll.kernel32.GetModuleHandleW(0),
    "shell32.dll", 
    0
)

# 将图标句柄转换成 IconHandle
icon_handle = pywintray.IconHandle.from_int(hicon)

... # 执行一些需要用到 icon_handle 的代码

# 在 icon_handle 的生命周期结束之后，记得回收 hicon
ctypes.windll.user32.DestroyIcon(hicon)
```

{{ API("TrayIcon", "托盘图标") }}

{{ API("TrayIcon.__init__", "初始化一个 `TrayIcon` 类") }}

{{ API("TrayIcon.register_callback", "注册托盘菜单的回调函数") }}

{{ API("TrayIcon.notify", "向用户弹出一条通知") }}

{{ API("Menu", "菜单的基类") }}

{{ API("Menu.popup", "弹出菜单") }}

{{ API("MenuItem", "菜单项目类") }}

{{ API("MenuItem.string", "创建一个`string`菜单项") }}

{{ API("MenuItem.check", "创建一个`check`菜单项") }}

{{ API("MenuItem.separator", "创建一个`separator`菜单项") }}

{{ API("MenuItem.submenu", "创建一个返回`submenu`菜单项的装饰器") }}

{{ API("MenuItem.label", "菜单项的标签") }}

{{ API("MenuItem.sub", "菜单项的子菜单") }}

{{ API("__version__", "版本号（字符串）") }}

{{ API("VERSION", "版本号（元组）") }}

# 类型注释

以下列出的变量仅用于类型注释，在运行时不存在。
请不要在运行时的代码里使用它们

{{ API("_TrayIconCallbackTypes", "") }}
