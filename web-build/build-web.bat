@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM Lupine Engine - Web Build Script (Windows Batch)
REM ============================================================

REM Parse arguments
set "DEBUG_BUILD=0"
set "CLEAN=0"
set "SERVE=0"
set "BUILD_TYPE=Release"

:parse_args
if "%~1"=="" goto :done_args
if /i "%~1"=="-debug" set "DEBUG_BUILD=1" & set "BUILD_TYPE=Debug"
if /i "%~1"=="--debug" set "DEBUG_BUILD=1" & set "BUILD_TYPE=Debug"
if /i "%~1"=="-clean" set "CLEAN=1"
if /i "%~1"=="--clean" set "CLEAN=1"
if /i "%~1"=="-serve" set "SERVE=1"
if /i "%~1"=="--serve" set "SERVE=1"
if /i "%~1"=="-help" goto :show_help
if /i "%~1"=="--help" goto :show_help
if /i "%~1"=="-h" goto :show_help
shift
goto :parse_args
:done_args

REM Script directories
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_ROOT=%%~fI"
set "BUILD_DIR=%SCRIPT_DIR%\build-web"
set "OUTPUT_DIR=%BUILD_DIR%\bin"
REM The debug template links to its own directory: its files are named index.*
REM exactly like the release template's and would otherwise overwrite them.
set "DEBUG_OUTPUT_DIR=%OUTPUT_DIR%\web_debug"

echo.
echo ============================================================
echo   Lupine Engine - Web Build
echo ============================================================
echo.

echo [*] Build type: %BUILD_TYPE%

REM Check for Emscripten
echo [*] Checking Emscripten...
where emcc >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [X] Emscripten not found in PATH!
    echo.
    echo     Please install and activate Emscripten SDK:
    echo       1. git clone https://github.com/emscripten-core/emsdk.git
    echo       2. cd emsdk
    echo       3. emsdk install latest
    echo       4. emsdk activate latest
    echo       5. emsdk_env.bat
    echo.
    exit /b 1
)
for /f "tokens=*" %%i in ('emcc --version 2^>^&1') do (
    echo     Found: %%i
    goto :emcc_done
)
:emcc_done

where emcmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [X] emcmake not found! Emscripten may not be properly activated.
    exit /b 1
)

REM Check for CMake
echo [*] Checking CMake...
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [X] CMake not found in PATH!
    exit /b 1
)
for /f "tokens=*" %%i in ('cmake --version') do (
    echo     Found: %%i
    goto :cmake_done
)
:cmake_done

REM Determine generator
set "GENERATOR="
where ninja >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "GENERATOR=Ninja"
    echo     Using Ninja generator
) else (
    set "GENERATOR=MinGW Makefiles"
    echo     Using MinGW Makefiles generator
)

REM Get processor count
set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" set "JOBS=4"
echo     Parallel jobs: %JOBS%

REM Check for Ruby (optional)
echo [*] Checking optional dependencies...
set "HAS_MRUBY=0"
where ruby >nul 2>&1
if %ERRORLEVEL% equ 0 (
    where rake >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        for /f "tokens=*" %%i in ('ruby --version 2^>^&1') do echo     Ruby found: %%i
        set "HAS_MRUBY=1"
    )
)
if "%HAS_MRUBY%"=="0" echo [!] Ruby/Rake not found - mRuby scripting will be disabled

REM Clean if requested
if "%CLEAN%"=="1" (
    echo [*] Cleaning build directory...
    if exist "%BUILD_DIR%" (
        rmdir /s /q "%BUILD_DIR%"
        echo     Removed: %BUILD_DIR%
    )
)

REM Create build directory
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    echo     Created: %BUILD_DIR%
)

REM Build mRuby if available
if "%HAS_MRUBY%"=="1" (
    set "MRUBY_ROOT=%PROJECT_ROOT%\external\mruby"
    set "MRUBY_LIB=!MRUBY_ROOT!\build\lupine-emscripten\lib\libmruby.a"
    set "LUPINE_CONFIG_SRC=%PROJECT_ROOT%\resources\lupine-emscripten.rb"
    set "LUPINE_CONFIG_DST=!MRUBY_ROOT!\build_config\lupine-emscripten.rb"

    echo [*] Checking mRuby build configuration...
    if exist "!LUPINE_CONFIG_SRC!" (
        if not exist "!LUPINE_CONFIG_DST!" (
            echo     Copying lupine-emscripten.rb to mRuby build_config...
            copy "!LUPINE_CONFIG_SRC!" "!LUPINE_CONFIG_DST!" >nul
        ) else (
            echo     mRuby config already in place
        )
    )

    if not exist "!MRUBY_LIB!" (
        echo [*] Building mRuby for Emscripten...
        pushd "!MRUBY_ROOT!"
        set "MRUBY_CONFIG=lupine-emscripten"
        call rake
        if !ERRORLEVEL! neq 0 (
            echo [!] mRuby host build failed - continuing without mRuby support
            set "HAS_MRUBY=0"
        ) else (
            call emmake rake
            if !ERRORLEVEL! neq 0 (
                echo [!] mRuby cross build failed - continuing without mRuby support
                set "HAS_MRUBY=0"
            )
        )
        popd
    ) else (
        echo     mRuby already built for Emscripten
    )
)

REM Configure CMake
echo [*] Configuring CMake...
set "CONFIG_FILE=%BUILD_DIR%\CMakeCache.txt"
set "NEEDS_CONFIGURE=0"
if not exist "%CONFIG_FILE%" set "NEEDS_CONFIGURE=1"
if "%CLEAN%"=="1" set "NEEDS_CONFIGURE=1"

if "%NEEDS_CONFIGURE%"=="1" (
    echo     Running: emcmake cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DLUPINE_BUILD_EXPORT_TEMPLATES=ON -DBUILD_PYTHON_MODULES=OFF

    pushd "%BUILD_DIR%"
    call emcmake cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DLUPINE_BUILD_EXPORT_TEMPLATES=ON -DBUILD_PYTHON_MODULES=OFF
    if !ERRORLEVEL! neq 0 (
        echo [X] CMake configuration failed!
        popd
        exit /b 1
    )
    popd
    echo     Configuration complete
) else (
    echo     Using existing configuration (use -clean to reconfigure)
)

REM Build. Both templates are produced every time: the release one that ships, and
REM the debug one (function names preserved in the wasm, assertions on) that the
REM editor uses when a preset enables "Include debug symbols".
echo [*] Building...
echo     Running: cmake --build "%BUILD_DIR%" --target lupine_template lupine_template_debug --config %BUILD_TYPE% -j %JOBS%

cmake --build "%BUILD_DIR%" --target lupine_template lupine_template_debug --config %BUILD_TYPE% -j %JOBS%
if %ERRORLEVEL% neq 0 (
    echo [X] Build failed!
    exit /b 1
)

REM Verify output
echo [*] Verifying output...
set "MISSING_FILES="
for %%f in (index.html index.js index.wasm) do (
    if exist "%DEBUG_OUTPUT_DIR%\%%f" (
        echo     web_debug\%%f : OK
    ) else (
        set "MISSING_FILES=!MISSING_FILES! web_debug\%%f"
    )
)
for %%f in (index.html index.js index.wasm) do (
    if exist "%OUTPUT_DIR%\%%f" (
        echo     %%f : OK
    ) else (
        set "MISSING_FILES=!MISSING_FILES! %%f"
    )
)

if defined MISSING_FILES (
    echo [X] Missing output files:%MISSING_FILES%
    exit /b 1
)

REM Install to export_templates
echo [*] Installing web templates...
set "EXPORT_TEMPLATES_DIR=%PROJECT_ROOT%\build\export_templates"
set "WEB_TEMPLATE_DIR=%EXPORT_TEMPLATES_DIR%\web"
REM The exporter looks for the debug variant in web\debug (see ExportPlatform.
REM debug_template_filename in editor/export/export_manager.py).
set "WEB_DEBUG_TEMPLATE_DIR=%WEB_TEMPLATE_DIR%\debug"

if not exist "%WEB_TEMPLATE_DIR%" (
    mkdir "%WEB_TEMPLATE_DIR%"
    echo     Created: %WEB_TEMPLATE_DIR%
)

if not exist "%WEB_DEBUG_TEMPLATE_DIR%" (
    mkdir "%WEB_DEBUG_TEMPLATE_DIR%"
    echo     Created: %WEB_DEBUG_TEMPLATE_DIR%
)

for %%f in (index.html index.js index.wasm) do (
    if exist "%OUTPUT_DIR%\%%f" (
        copy /y "%OUTPUT_DIR%\%%f" "%WEB_TEMPLATE_DIR%\%%f" >nul
        echo     Installed: %%f
    )
)

if exist "%OUTPUT_DIR%\index.data" (
    copy /y "%OUTPUT_DIR%\index.data" "%WEB_TEMPLATE_DIR%\index.data" >nul
    echo     Installed: index.data
)

REM index.wasm.symbols is the --emit-symbol-map output used to demangle wasm frames.
for %%f in (index.html index.js index.wasm index.data index.wasm.symbols) do (
    if exist "%DEBUG_OUTPUT_DIR%\%%f" (
        copy /y "%DEBUG_OUTPUT_DIR%\%%f" "%WEB_DEBUG_TEMPLATE_DIR%\%%f" >nul
        echo     Installed: debug\%%f
    )
)

REM Copy default Lupine favicon
set "LUPINE_ICON=%PROJECT_ROOT%\editor\resources\icon.png"
set "FAVICON_DST=%WEB_TEMPLATE_DIR%\favicon.ico"
if exist "%LUPINE_ICON%" (
    REM Try to convert PNG to ICO using Python with PIL
    where python >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        python -c "from PIL import Image; img = Image.open(r'%LUPINE_ICON%'); img.save(r'%FAVICON_DST%', format='ICO', sizes=[(16,16), (32,32), (48,48), (64,64)]); print('OK')" 2>nul | findstr /C:"OK" >nul
        if !ERRORLEVEL! equ 0 (
            echo     Installed: favicon.ico (converted from PNG^)
        ) else (
            REM Fall back to PNG favicon
            copy /y "%LUPINE_ICON%" "%WEB_TEMPLATE_DIR%\favicon.png" >nul
            echo     Installed: favicon.png (PNG fallback^)
        )
    ) else (
        REM No Python, copy PNG directly
        copy /y "%LUPINE_ICON%" "%WEB_TEMPLATE_DIR%\favicon.png" >nul
        echo     Installed: favicon.png (PNG fallback^)
    )
) else (
    echo [!] Lupine icon not found at: %LUPINE_ICON%
)

REM Update templates.json using PowerShell (simpler JSON handling)
echo [*] Updating templates manifest...
powershell -NoProfile -Command ^
    "$jsonPath = '%EXPORT_TEMPLATES_DIR%\templates.json'; " ^
    "$data = if (Test-Path $jsonPath) { Get-Content $jsonPath -Raw | ConvertFrom-Json } else { @{version='0.1.0';templates=@{}} }; " ^
    "if (-not $data.templates) { $data | Add-Member -NotePropertyName templates -NotePropertyValue @{} -Force }; " ^
    "$data.templates | Add-Member -NotePropertyName web -NotePropertyValue @{platform='web';architecture='wasm32';binary='index.html';debug_binary='debug/index.html';graphics_backends=@('webgl')} -Force; " ^
    "$data | ConvertTo-Json -Depth 4 | Set-Content $jsonPath -Encoding UTF8"
echo     Updated: %EXPORT_TEMPLATES_DIR%\templates.json

echo.
echo ============================================================
echo   Build Successful!
echo ============================================================
echo.
echo     Output directory: %OUTPUT_DIR%
echo     Template installed to: %WEB_TEMPLATE_DIR%
echo     Debug template installed to: %WEB_DEBUG_TEMPLATE_DIR%
echo     (function names preserved; used when a preset enables "Include debug symbols")
echo.

if "%SERVE%"=="1" (
    echo [*] Starting local test server...
    if exist "%SCRIPT_DIR%\serve.py" (
        where python >nul 2>&1
        if !ERRORLEVEL! equ 0 (
            echo     Server URL: http://localhost:8080/
            echo     Press Ctrl+C to stop
            python "%SCRIPT_DIR%\serve.py" "%OUTPUT_DIR%"
        ) else (
            echo [!] Python not found - cannot start server
            echo     Start manually: python -m http.server 8080 --directory "%OUTPUT_DIR%"
        )
    ) else (
        echo [!] serve.py not found
        echo     Start manually: python -m http.server 8080 --directory "%OUTPUT_DIR%"
    )
) else (
    echo     To test locally:
    echo       build-web.bat -serve
    echo       or: python serve.py
)

exit /b 0

:show_help
echo.
echo Lupine Engine - Web Build Script
echo.
echo Usage: build-web.bat [options]
echo.
echo Options:
echo   -debug, --debug     Build with debug symbols and assertions
echo   -clean, --clean     Clean the build directory before building
echo   -serve, --serve     Start a local test server after building
echo   -help, --help, -h   Show this help message
echo.
echo Examples:
echo   build-web.bat                  Build release template
echo   build-web.bat -debug -clean    Clean and build debug template
echo   build-web.bat -serve           Build and start test server
echo.
exit /b 0
