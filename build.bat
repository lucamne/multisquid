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
set CFLAGS=/nologo /W4 /GL /DWINDOWS

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

REM compile
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
