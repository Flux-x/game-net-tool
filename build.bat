@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"
set "ROOT=%~dp0"
set "OUT=%ROOT%bin"

set "VCVARS="
for %%V in (18 17 16) do (
    for %%E in (Community Professional Enterprise BuildTools) do (
        if not defined VCVARS (
            if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat" (
                set "VCVARS=C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat"
            )
        )
    )
)

if not defined VCVARS (
    echo [ERROR] Visual Studio C++ build tools not found.
    exit /b 1
)

if not exist "%OUT%" mkdir "%OUT%"

echo [*] Initializing Visual Studio x64 environment...
call "%VCVARS%" x64 >nul
if errorlevel 1 (
    echo [ERROR] Failed to set up vcvarsall.
    exit /b 1
)

echo [*] Compiling tcp_stress.exe...
cl /nologo /O2 /std:c++17 /EHsc /W3 /DNDEBUG /MT ^
    "%ROOT%src\tcp_tool.cpp" ^
    /Fe:"%OUT%\tcp_stress.exe" ^
    /Fo:"%OUT%\tcp_stress.obj" ^
    /link /SUBSYSTEM:CONSOLE Ws2_32.lib

if errorlevel 1 (
    echo [ERROR] Compilation failed.
    exit /b 1
)

del /q "%OUT%\*.obj" >nul 2>&1
echo [SUCCESS] Binary created: bin\tcp_stress.exe
