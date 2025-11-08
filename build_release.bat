@echo off
setlocal enabledelayedexpansion

pushd "%~dp0" >nul

set "TOOLCHAIN=external\vcpkg\scripts\buildsystems\vcpkg.cmake"

set "ARCH_INPUT=%~1"
set "TRIPLET=%VCPKG_TARGET_TRIPLET%"
if not defined TRIPLET set "TRIPLET=x64-mingw-dynamic"

if defined ARCH_INPUT (
    if /I "%ARCH_INPUT%"=="x86" (
        set "TRIPLET=x86-mingw-dynamic"
    ) else if /I "%ARCH_INPUT%"=="x64" (
        set "TRIPLET=x64-mingw-dynamic"
    ) else (
        set "TRIPLET=%ARCH_INPUT%"
    )
)

for /f "tokens=1 delims=-" %%A in ("%TRIPLET%") do set "ARCH_TAG=%%A"
if not defined ARCH_TAG set "ARCH_TAG=x64"

set "BUILD_DIR=build\release-%TRIPLET%"
set "DIST_ROOT=dist\windows-%ARCH_TAG%"
set "PACKAGE_STAGE=%DIST_ROOT%\demo-release"
set "PACKAGE_ARCHIVE=%DIST_ROOT%\demo-windows-%ARCH_TAG%-release.zip"

set "GENERATOR=%CMAKE_GENERATOR%"
if not defined GENERATOR (
    set "GENERATOR=Ninja"
)

if /I "%GENERATOR%"=="Ninja" (
    where.exe ninja >nul 2>&1
    if %errorlevel% neq 0 (
        echo [WARN] Ninja executable not found in PATH. Falling back to Visual Studio generator.
        set "GENERATOR=Visual Studio 17 2022"
    )
)

set "GENERATOR_ARCH_SWITCH="
if /I "%GENERATOR%"=="Visual Studio 17 2022" (
    if /I "%ARCH_TAG%"=="x86" (
        set "GENERATOR_ARCH_SWITCH=-A Win32"
    ) else (
        set "GENERATOR_ARCH_SWITCH=-A x64"
    )
)

echo Using CMake generator: %GENERATOR%

echo ========================================
echo Building release configuration
echo ========================================

if exist "%BUILD_DIR%" cmake -E remove_directory "%BUILD_DIR%"
if not exist "%DIST_ROOT%" cmake -E make_directory "%DIST_ROOT%"

if defined GENERATOR_ARCH_SWITCH (
    cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" %GENERATOR_ARCH_SWITCH% -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%" -DVCPKG_TARGET_TRIPLET="%TRIPLET%"
) else (
    cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%" -DVCPKG_TARGET_TRIPLET="%TRIPLET%"
)
if errorlevel 1 goto :error

cmake --build "%BUILD_DIR%"
if errorlevel 1 goto :error

set "EXECUTABLE=%BUILD_DIR%\demo.exe"
if not exist "%EXECUTABLE%" (
    echo [ERROR] Expected executable not found at %EXECUTABLE%
    goto :error
)

cmake -E remove_directory "%PACKAGE_STAGE%"
cmake -E make_directory "%PACKAGE_STAGE%"

cmake -E copy "%EXECUTABLE%" "%PACKAGE_STAGE%\demo.exe"
cmake -E copy_directory "assets" "%PACKAGE_STAGE%\assets"
cmake -E copy_if_different "README.md" "%PACKAGE_STAGE%\README.md"

for %%F in ("%BUILD_DIR%\*.dll") do (
    if exist "%%~fF" cmake -E copy_if_different "%%~fF" "%PACKAGE_STAGE%"
)

for %%F in ("%BUILD_DIR%\*.pdb") do (
    if exist "%%~fF" cmake -E copy_if_different "%%~fF" "%PACKAGE_STAGE%"
)

for %%L in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    set "FOUND_DLL="
    for /f "delims=" %%P in ('g++ -print-file-name=%%L 2^>nul') do (
        if exist "%%~fP" (
            set "FOUND_DLL=%%~fP"
        )
    )
    if not defined FOUND_DLL (
        for /f "delims=" %%P in ('where %%L 2^>nul') do (
            if exist "%%~fP" if not defined FOUND_DLL set "FOUND_DLL=%%~fP"
        )
    )
    if defined FOUND_DLL (
        cmake -E copy_if_different "!FOUND_DLL!" "%PACKAGE_STAGE%"
    ) else (
        echo [WARN] Required runtime %%L not found in PATH.
    )
)

if exist "%PACKAGE_ARCHIVE%" del "%PACKAGE_ARCHIVE%" >nul 2>&1
powershell -NoLogo -NoProfile -Command "Compress-Archive -Path '%PACKAGE_STAGE%\*' -DestinationPath '%PACKAGE_ARCHIVE%' -Force"
if errorlevel 1 goto :error

echo.
echo [SUCCESS] Release package available at %PACKAGE_ARCHIVE%
goto :eof

:error
echo.
echo [FAILED] Release build or packaging step encountered an error.
exit /b 1

