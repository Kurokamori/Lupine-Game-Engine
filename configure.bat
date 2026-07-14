@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
    echo FAILED to set up MSVC environment
    exit /b 1
)
echo MSVC environment configured
where cl
cd /d H:\Programming\Engine\Trial-and-Error\Lupine-Engine
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S .
