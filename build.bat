@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
echo INCLUDE=%INCLUDE%
cd /d H:\Programming\Engine\Trial-and-Error\Lupine-Engine\build

set "LOG_DIR=H:\Programming\Engine\Trial-and-Error\Lupine-Engine\build\logs"
set "FULL_LOG=%LOG_DIR%\build_output.log"
set "WARN_LOG=%LOG_DIR%\build_warnings.log"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

cmake .. -DLUPINE_BUILD_EXPORT_TEMPLATES=ON
cmake --build . --config Release 2>&1 | powershell -NoProfile -Command "$input | Tee-Object -FilePath '%FULL_LOG%'"

powershell -NoProfile -ExecutionPolicy Bypass -File "H:\Programming\Engine\Trial-and-Error\Lupine-Engine\cmake\collect_warnings.ps1" -InputLog "%FULL_LOG%" -OutputLog "%WARN_LOG%"

echo.
echo Full build log:     %FULL_LOG%
echo Warning/error log:  %WARN_LOG%
endlocal
