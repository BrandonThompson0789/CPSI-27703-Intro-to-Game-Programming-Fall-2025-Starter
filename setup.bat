@echo off
setlocal enabledelayedexpansion

echo ========================================
echo SDL2 + Box2D + tinyxml2 Setup Script
echo ========================================
echo.

echo Checking prerequisites...
echo.

:: Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo [ERROR] CMakeLists.txt not found. Please run this script from the project root directory.
    pause
    exit /b 1
)

:: Check Git
echo Checking Git...
git --version >nul 2>&1
if !errorlevel! equ 0 (
    echo [SUCCESS] Git is installed
) else (
    echo [ERROR] Git is not installed or not in PATH
    echo [INFO] Download from: https://git-scm.com/download/win
    pause
    exit /b 1
)

:: Check CMake
echo Checking CMake...
cmake --version >nul 2>&1
if !errorlevel! equ 0 (
    echo [SUCCESS] CMake is installed
) else (
    echo [WARNING] CMake not found
    echo [INFO] Attempting to install CMake using Chocolatey...
    
    :: Check if Chocolatey is installed
    where choco >nul 2>&1
    if !errorlevel! equ 0 (
        echo [INFO] Chocolatey found. Installing CMake...
        choco install cmake -y
        if !errorlevel! equ 0 (
            echo [SUCCESS] CMake installed successfully
            echo [INFO] Please restart your terminal/VS Code to refresh PATH
            echo.
            set /p "CONTINUE=Continue with setup? (y/n): "
            if /i not "!CONTINUE!"=="y" (
                pause
                exit /b 1
            )
        ) else (
            echo [ERROR] Failed to install CMake via Chocolatey
            echo [INFO] Please install manually from: https://cmake.org/download/
            pause
            exit /b 1
        )
    ) else (
        echo [WARNING] Chocolatey not found
        echo [INFO] Installing Chocolatey package manager...
        
        :: Install Chocolatey
        powershell -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
        
        if !errorlevel! equ 0 (
            echo [SUCCESS] Chocolatey installed successfully
            echo [INFO] Installing CMake...
            
            :: Refresh PATH and install CMake
            call choco install cmake -y
            if !errorlevel! equ 0 (
                echo [SUCCESS] CMake installed successfully
                echo [INFO] Please restart your terminal/VS Code to refresh PATH
                echo.
                set /p "CONTINUE=Continue with setup? (y/n): "
                if /i not "!CONTINUE!"=="y" (
                    pause
                    exit /b 1
                )
            ) else (
                echo [ERROR] Failed to install CMake via Chocolatey
                echo [INFO] Please install manually from: https://cmake.org/download/
                pause
                exit /b 1
            )
        ) else (
            echo [ERROR] Failed to install Chocolatey
            echo [INFO] Please install CMake manually from: https://cmake.org/download/
            pause
            exit /b 1
        )
    )
)

:: Check MinGW-w64
echo Checking MinGW-w64...
where gcc >nul 2>&1
if !errorlevel! equ 0 (
    echo [SUCCESS] MinGW-w64 GCC is installed
) else (
    echo [WARNING] MinGW-w64 GCC not found
    echo [INFO] Attempting to install MinGW-w64 using Chocolatey...
    
    :: Check if Chocolatey is installed
    where choco >nul 2>&1
    if !errorlevel! equ 0 (
        echo [INFO] Chocolatey found. Installing MinGW-w64...
        choco install mingw -y
        if !errorlevel! equ 0 (
            echo [SUCCESS] MinGW-w64 installed successfully
            echo [INFO] Please restart your terminal/VS Code to refresh PATH
            echo.
            set /p "CONTINUE=Continue with setup? (y/n): "
            if /i not "!CONTINUE!"=="y" (
                pause
                exit /b 1
            )
        ) else (
            echo [ERROR] Failed to install MinGW-w64 via Chocolatey
            echo [INFO] Please install manually from: https://www.mingw-w64.org/downloads/
            pause
            exit /b 1
        )
    ) else (
        echo [WARNING] Chocolatey not found
        echo [INFO] Installing Chocolatey package manager...
        
        :: Install Chocolatey
        powershell -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
        
        if !errorlevel! equ 0 (
            echo [SUCCESS] Chocolatey installed successfully
            echo [INFO] Installing MinGW-w64...
            
            :: Refresh PATH and install MinGW
            call choco install mingw -y
            if !errorlevel! equ 0 (
                echo [SUCCESS] MinGW-w64 installed successfully
                echo [INFO] Please restart your terminal/VS Code to refresh PATH
                echo.
                set /p "CONTINUE=Continue with setup? (y/n): "
                if /i not "!CONTINUE!"=="y" (
                    pause
                    exit /b 1
                )
            ) else (
                echo [ERROR] Failed to install MinGW-w64 via Chocolatey
                echo [INFO] Please install manually from: https://www.mingw-w64.org/downloads/
                pause
                exit /b 1
            )
        ) else (
            echo [ERROR] Failed to install Chocolatey
            echo [INFO] Please install MinGW-w64 manually from: https://www.mingw-w64.org/downloads/
            pause
            exit /b 1
        )
    )
)

:: Check Ninja
echo Checking Ninja...
where ninja >nul 2>&1
if !errorlevel! equ 0 (
    echo [SUCCESS] Ninja is installed
) else (
    echo [WARNING] Ninja not found
    echo [INFO] Attempting to install Ninja using Chocolatey...
    
    :: Check if Chocolatey is installed
    where choco >nul 2>&1
    if !errorlevel! equ 0 (
        echo [INFO] Chocolatey found. Installing Ninja...
        choco install ninja -y
        if !errorlevel! equ 0 (
            echo [SUCCESS] Ninja installed successfully
            echo [INFO] Please restart your terminal/VS Code to refresh PATH
            echo.
            set /p "CONTINUE=Continue with setup? (y/n): "
            if /i not "!CONTINUE!"=="y" (
                pause
                exit /b 1
            )
        ) else (
            echo [ERROR] Failed to install Ninja via Chocolatey
            echo [INFO] Please install manually from: https://ninja-build.org/
            pause
            exit /b 1
        )
    ) else (
        echo [WARNING] Chocolatey not found
        echo [INFO] Installing Chocolatey package manager...
        
        :: Install Chocolatey
        powershell -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
        
        if !errorlevel! equ 0 (
            echo [SUCCESS] Chocolatey installed successfully
            echo [INFO] Installing Ninja...
            
            :: Refresh PATH and install Ninja
            call choco install ninja -y
            if !errorlevel! equ 0 (
                echo [SUCCESS] Ninja installed successfully
                echo [INFO] Please restart your terminal/VS Code to refresh PATH
                echo.
                set /p "CONTINUE=Continue with setup? (y/n): "
                if /i not "!CONTINUE!"=="y" (
                    pause
                    exit /b 1
                )
            ) else (
                echo [ERROR] Failed to install Ninja via Chocolatey
                echo [INFO] Please install manually from: https://ninja-build.org/
                pause
                exit /b 1
            )
        ) else (
            echo [ERROR] Failed to install Chocolatey
            echo [INFO] Please install Ninja manually from: https://ninja-build.org/
            pause
            exit /b 1
        )
    )
)

echo.
echo [SUCCESS] All prerequisites are met!
echo.

:: Check if vcpkg submodule exists
if not exist "external\vcpkg" (
    echo [WARNING] vcpkg submodule not found. Initializing...
    echo.
    echo Initializing vcpkg submodule...
    git submodule update --init --recursive external/vcpkg
    if !errorlevel! equ 0 (
        echo [SUCCESS] vcpkg submodule initialized
    ) else (
        echo [ERROR] Failed to initialize vcpkg submodule
        echo [INFO] Make sure you cloned the repository with: git clone --recursive
        pause
        exit /b 1
    )
) else (
    if not exist "external\vcpkg\.git" (
        echo [WARNING] vcpkg submodule directory exists but is not initialized. Initializing...
        echo.
        git submodule update --init --recursive external/vcpkg
        if !errorlevel! equ 0 (
            echo [SUCCESS] vcpkg submodule initialized
        ) else (
            echo [ERROR] Failed to initialize vcpkg submodule
            echo [INFO] Make sure you cloned the repository with: git clone --recursive
            pause
            exit /b 1
        )
    ) else if not exist "external\vcpkg\bootstrap-vcpkg.bat" (
        echo [WARNING] vcpkg submodule appears incomplete. Updating...
        echo.
        git submodule update --init --recursive external/vcpkg
        if !errorlevel! equ 0 (
            echo [SUCCESS] vcpkg submodule updated
        ) else (
            echo [ERROR] Failed to update vcpkg submodule
            pause
            exit /b 1
        )
    ) else (
        echo [SUCCESS] vcpkg submodule already initialized
    )
)

:: Check if vcpkg is bootstrapped
if not exist "external\vcpkg\vcpkg.exe" (
    echo [WARNING] vcpkg not bootstrapped. Bootstrapping...
    echo.
    echo Bootstrapping vcpkg...
    cd external\vcpkg
    call bootstrap-vcpkg.bat
            if !errorlevel! equ 0 (
        echo [SUCCESS] vcpkg bootstrapped successfully
    ) else (
        echo [ERROR] Failed to bootstrap vcpkg
        pause
        exit /b 1
    )
    cd ..\..
) else (
    echo [SUCCESS] vcpkg is already bootstrapped
)

:: Configure CMake
echo.
echo Configuring CMake...
cmake --preset windows-mingw-debug
        if !errorlevel! equ 0 (
    echo [SUCCESS] CMake configuration completed
) else (
    echo [ERROR] CMake configuration failed
    echo [INFO] Check the error messages above for details
    pause
    exit /b 1
)

:: Build the project
echo.
echo Building the project...
cmake --build build/win-mingw-debug
            if !errorlevel! equ 0 (
    echo [SUCCESS] Build completed successfully
) else (
    echo [ERROR] Build failed
    echo [INFO] Check the error messages above for details
    pause
    exit /b 1
)

:: Check if executable was created
if exist "build\win-mingw-debug\demo.exe" (
    echo [SUCCESS] Executable created successfully
) else (
    echo [ERROR] Executable not found after build
    pause
    exit /b 1
)

echo.
echo ========================================
echo [SUCCESS] SETUP COMPLETED SUCCESSFULLY!
echo ========================================
echo.
echo [INFO] You can now run the application with:
echo   .\build\win-mingw-debug\demo.exe
echo.
echo [INFO] Or press F5 in VS Code/Cursor to build and debug with GDB
echo.
echo [INFO] Next steps:
echo   1. Install C/C++ Extension Pack in VS Code/Cursor
echo   2. Open the project in your editor
echo   3. Start coding!
echo.

pause
