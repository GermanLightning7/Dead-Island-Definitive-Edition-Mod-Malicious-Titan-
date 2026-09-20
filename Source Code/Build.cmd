@echo off
setlocal
cd /d "%~dp0"
call Source\Build.cmd
if errorlevel 1 exit /b 1
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File Update-LauncherHashes.ps1
if errorlevel 1 exit /b 1
"%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:winexe /platform:x64 /optimize+ /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /win32manifest:Launcher\app.manifest /out:"Builds\Play Jenta's Special Edition.exe" Launcher\ManualLauncher.cs
exit /b %errorlevel%
