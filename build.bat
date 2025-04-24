@echo off

set build_path=.\build
set src_path=.\src_c

call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64

if not exist %build_path% (
    mkdir %build_path%
)

set compile_args=/Fo"%build_path%/" /Fd"%build_path%/" /Fe"%build_path%/" ^
    /I"%python_path%/include" /I"%src_path%/include" ^
    /c /O2 /W3 /WX /GS-

cl %compile_args% %src_path%/tray_icon.c

cl %compile_args% %src_path%/pywintray.c

cl %compile_args% %src_path%/icon_handle.c

link /LIBPATH:"%python_path%\libs" /OUT:"%build_path%\pywintray.pyd" ^
    /NODEFAULTLIB:msvcrt /ENTRY:DllMain /DLL ^
    %build_path%\pywintray.obj %build_path%\icon_handle.obj %build_path%\tray_icon.obj

if %errorlevel% == 0 (
    copy "%build_path%\pywintray.pyd" ".\"
)
