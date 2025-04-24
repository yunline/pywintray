@echo off

set build_path=.\build

call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64

if not exist %build_path% (
    mkdir %build_path%
)

cl /Fo"%build_path%/" /Fd"%build_path%/" /Fe"%build_path%/" /I"%python_path%\include" ^
    /c /LD /MD /O2 /W3 /GS- /WX pywintray.c

cl /Fo"%build_path%/" /Fd"%build_path%/" /Fe"%build_path%/" /I"%python_path%\include" ^
    /c /LD /MD /O2 /W3 /GS- /WX icon_handle.c

link /LIBPATH:"%python_path%\libs" /NODEFAULTLIB:msvcrt /ENTRY:DllMain /DLL /OUT:"%build_path%\pywintray.pyd" ^
    %build_path%\pywintray.obj %build_path%\icon_handle.obj

if %errorlevel% == 0 (
    copy "%build_path%\pywintray.pyd" ".\"
)
