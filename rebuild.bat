@echo off
setlocal
cd /d "%~dp0"

rem ===================================================================
rem  XiaoLiZiVM Python SDK - one-click rebuild launcher
rem
rem  Calls tools\rebuild_all.py, which:
rem    1. builds PythonSDK\PythonCompatPlugin.sln   (Release^|Win32)
rem    2. builds xlz_pyruntime\xlz_pyruntime.sln    (Release^|Win32)
rem    3. deploys xlz_pyruntime.dll into ..\..\main\corn\
rem    4. repacks the four demo plugin DLLs
rem    5. verifies the PE export table against exports.def
rem    6. assembles the Releases\ author kit (skipped when step 5 fails)
rem
rem  Any extra arguments are passed through, e.g.:
rem      rebuild.bat --dry-run
rem      rebuild.bat --skip-build
rem      rebuild.bat --releases-only
rem ===================================================================

set "PYEXE="
if exist "%~dp0..\..\main\corn\python\python.exe" set "PYEXE=%~dp0..\..\main\corn\python\python.exe"
if not defined PYEXE for %%P in (python.exe) do set "PYEXE=%%~$PATH:P"
if not defined PYEXE goto nopy

echo.
echo  Rebuilding XiaoLiZiVM Python SDK ...
echo  Python : %PYEXE%
echo.
"%PYEXE%" "%~dp0tools\rebuild_all.py" %*
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" echo  [OK] Rebuild finished successfully.
if not "%RC%"=="0" echo  [FAILED] exit code %RC%
echo.
pause
exit /b %RC%

:nopy
echo.
echo  [ERROR] Python not found.
echo          Expected bundled runtime: ..\..\main\corn\python\python.exe
echo          Or add python.exe to PATH and run again.
echo.
pause
exit /b 1
