@echo off
setlocal

set ARCH=x64
if "%1"=="x86" set ARCH=x86
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -A %ARCH%
cmake --build build --config Release --verbose