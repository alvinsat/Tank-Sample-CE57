@echo off
setlocal enabledelayedexpansion

set "CACHE_FILE=%TEMP%\ce_cmake_path.txt"
set "CMAKE_EXE="

REM --- 1. Check Cache ---
if exist "%CACHE_FILE%" (
    set /p CACHED_PATH=<"%CACHE_FILE%"
    if exist "!CACHED_PATH!" (
        set "CMAKE_EXE=!CACHED_PATH!"
        echo [CACHE] Found valid CMake path in temp.
        goto :START_BUILD
    ) else (
        echo [CACHE] Cached path is invalid. Re-searching...
        del "%CACHE_FILE%"
    )
)

REM --- 2. Quick Check (System PATH) ---
where cmake.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    for /f "delims=" %%i in ('where cmake.exe') do set "CMAKE_EXE=%%i"
    goto :SAVE_AND_BUILD
)

REM --- 3. Quick Check (Standard Launcher Path) ---
set "STD_PATH=%LOCALAPPDATA%\Programs\CRYENGINE Launcher\Engines\crytek\cryengine-57-lts\5.7.1\Tools\CMake\Win32\bin\cmake.exe"
if exist "%STD_PATH%" (
    set "CMAKE_EXE=%STD_PATH%"
    goto :SAVE_AND_BUILD
)

REM --- 4. Deep Search (AppData) ---
echo Searching for cmake.exe (this may take a moment)...
for /r "%LOCALAPPDATA%\Programs" %%f in (cmake.exe) do (
    if "%%~nxf"=="cmake.exe" (
        set "CMAKE_EXE=%%f"
        goto :SAVE_AND_BUILD
    )
)

if not defined CMAKE_EXE (
    echo.
    echo ========================================
    echo ERROR: cmake.exe not found!
    echo ========================================
    pause
    exit /b 1
)

:SAVE_AND_BUILD
echo %CMAKE_EXE%>"%CACHE_FILE%"
echo [SEARCH] Found and cached: "%CMAKE_EXE%"

:START_BUILD
echo.
echo ========================================
echo Executing Build...
echo ========================================
"%CMAKE_EXE%" --build "solutions/win64" --config Profile --target Game

REM --- Capture the Build Result ---
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo SUCCESS: Build completed successfully!
    echo ========================================
) else (
    echo.
    echo ========================================
    echo FAILED: Build failed with Error Code %ERRORLEVEL%
    echo WARNING: Game.dll may not have been updated.
    echo ========================================
)

echo.
pause