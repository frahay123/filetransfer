@echo off
REM Build Windows Installer for PhotoTransfer
REM Requires: Visual Studio, Qt, NSIS

setlocal enabledelayedexpansion

echo === Building PhotoTransfer Windows Installer ===

set PROJECT_ROOT=%~dp0..\..
set BUILD_DIR=%PROJECT_ROOT%\build-windows
set DIST_DIR=%BUILD_DIR%\dist

REM Clean build directory
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Configure with CMake
echo Configuring project...
cmake "%PROJECT_ROOT%" ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_GUI=ON

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

REM Build
echo Building project...
cmake --build . --config Release

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

REM Create distribution directory
echo Creating distribution...
mkdir "%DIST_DIR%"
copy Release\photo_transfer.exe "%DIST_DIR%\"
copy Release\photo_transfer_gui.exe "%DIST_DIR%\"

REM Deploy Qt dependencies
echo Deploying Qt...
where windeployqt >nul 2>&1
if errorlevel 1 (
    where windeployqt6 >nul 2>&1
    if errorlevel 1 (
        echo Warning: windeployqt not found. Qt DLLs not bundled.
    ) else (
        windeployqt6 --release "%DIST_DIR%\photo_transfer_gui.exe"
    )
) else (
    windeployqt --release "%DIST_DIR%\photo_transfer_gui.exe"
)

REM Copy additional dependencies (libmtp, etc.)
REM You may need to adjust these paths based on your system
if exist "C:\vcpkg\installed\x64-windows\bin\libmtp-9.dll" (
    copy "C:\vcpkg\installed\x64-windows\bin\libmtp*.dll" "%DIST_DIR%\"
)

REM Copy NSIS script and resources
copy "%~dp0installer.nsi" "%DIST_DIR%\"
copy "%~dp0icon.ico" "%DIST_DIR%\" 2>nul

REM Build installer
cd /d "%DIST_DIR%"
where makensis >nul 2>&1
if errorlevel 1 (
    echo Warning: NSIS not found. Skipping installer creation.
    echo Creating portable ZIP instead...
    powershell Compress-Archive -Path * -DestinationPath "%PROJECT_ROOT%\PhotoTransfer-1.0.0-Windows-Portable.zip" -Force
) else (
    echo Building NSIS installer...
    makensis installer.nsi
    move PhotoTransfer-*.exe "%PROJECT_ROOT%\"
)

echo.
echo === Build Complete ===
echo Output: %PROJECT_ROOT%\PhotoTransfer-1.0.0-Windows-*.exe

endlocal
