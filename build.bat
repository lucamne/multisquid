@echo off
setlocal ENABLEDELAYEDEXPANSION

REM set sources and outputs
set SOURCES=src\main.c src\engine.c
set INCLUDE=%INCLUDE% /Isrc
set OUTPUT=bin\multisquid.exe

REM release build
if "%~1"=="-r" (
    set RELEASE=1
) else (
    set RELEASE=0
)

REM Profiling build
if "%~1"=="-p" (
    set PROFILE=1
) else (
    set PROFILE=0
)

REM set flags
set CFLAGS=/nologo /W4 /DENGINE_VERSION=\"0.1.0\" /GL

if "%RELEASE%"=="1" (
    set CFLAGS=%CFLAGS% /O2 /DNDEBUG
    echo Building release

) else if "%PROFILE%"=="1" (
    set CFLAGS=%CFLAGS% /Zi /DNDEBUG /O2
    echo Building profiling

) else (
    set CFLAGS=%CFLAGS% /Zi
    echo Building Debug
)

REM ensure correct directory and compile
REM ******Uncomment this for work pc****** 
cd C:\Users\lnegris\source\repos\multisquid

REM ******This is for Luca's XPS laptop
REM cd C:\Users\negri\source\repos\multisquid

cl %CFLAGS% %SOURCES% /Fe:"%OUTPUT%" /link /INCREMENTAL:NO
if errorlevel 1 (
    echo Build failed
    exit /b 1
) else (
	echo Build complete^^!
)

REM cleanup object files and .pdb files if in release mode
del /q *.obj 2>nul
del /q "bin\*.obj" 2>nul
if "%RELEASE%"=="1" (
    del /q *.pdb 2>nul
    del /q "bin\*.pdb" 2>nul
)

endlocal
