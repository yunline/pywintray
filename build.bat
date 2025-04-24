@echo off

set build_path=.\build

call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64

if not exist %build_path% (
    mkdir %build_path%
)

cl /Fo"%build_path%/" /Fd"%build_path%/" /Fe"%build_path%/" /I"%python_path%\include" ^
    /LD /MD /O2 /W3 /GS- /WX pywintray.c ^
    /link /LIBPATH:"%python_path%\libs" /NODEFAULTLIB:msvcrt /ENTRY:DllMain

if %errorlevel% == 0 (
    copy "%build_path%\pywintray.dll" ".\pywintray.pyd"
)
