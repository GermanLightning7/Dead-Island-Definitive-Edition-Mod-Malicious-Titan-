@echo off
setlocal
cd /d "%~dp0"
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "DIDE_VS=%%i"
if not defined DIDE_VS exit /b 1
call "%DIDE_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
set "MH=%CD%\NativeMenu\third_party\MinHook"
if not exist ..\Builds mkdir ..\Builds
pushd ..\Builds
cl /nologo /O2 /MD /W3 /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /LD /I"%MH%\include" /I"%MH%\src" ..\Source\NativeMenu\src\Main\PersistentCollisionRedirect.cpp "%MH%\src\buffer.c" "%MH%\src\hook.c" "%MH%\src\trampoline.c" "%MH%\src\hde\hde64.c" /link /OUT:JentaSpecialEdition.dll user32.lib advapi32.lib d3d11.lib dxgi.lib d3dcompiler.lib dxguid.lib gdi32.lib shell32.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /MD /W3 /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /LD /I"%MH%\include" /I"%MH%\src" ..\Source\NativeMenu\src\Melee\MeleeAddon.cpp "%MH%\src\buffer.c" "%MH%\src\hook.c" "%MH%\src\trampoline.c" "%MH%\src\hde\hde64.c" /link /OUT:DideMeleeNative.dll user32.lib advapi32.lib d3d11.lib dxgi.lib d3dcompiler.lib dxguid.lib gdi32.lib shell32.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /MD /W3 /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /LD /I"%MH%\include" /I"%MH%\src" ..\Source\NativeMenu\src\Durability\DurabilityAddon.cpp "%MH%\src\buffer.c" "%MH%\src\hook.c" "%MH%\src\trampoline.c" "%MH%\src\hde\hde64.c" /link /OUT:DideMeleeDurability.dll user32.lib advapi32.lib d3d11.lib dxgi.lib d3dcompiler.lib dxguid.lib gdi32.lib shell32.lib
if errorlevel 1 exit /b 1
exit /b 0
