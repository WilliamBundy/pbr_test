@echo off
set msvcdir="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\"
if not defined DevEnvDir call %msvcdir%vcvars64.bat >nul

set game=pbr_test.exe

taskkill /IM %game% >nul 2>&1

nmake /nologo -f windows.mak
if "%~1"=="run" goto run
goto end

:run
start bin\%game%

:end
