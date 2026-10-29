@echo off
setlocal
set "GXX=g++"
where g++ >nul 2>nul || set "GXX=C:\Program Files\CodeBlocks\MinGW\bin\g++.exe"

"%GXX%" -std=c++17 -O2 -DUNICODE -D_UNICODE -Wall -Wextra ^
  main.cpp -o ScreenGuard.exe ^
  -mwindows -static -static-libgcc -static-libstdc++ ^
  -lgdi32 -luser32 -lshell32 -lole32 -lmfplat -lmfreadwrite -lmf -lmfuuid -lwindowscodecs -luuid
if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)
echo BUILD OK: ScreenGuard.exe
