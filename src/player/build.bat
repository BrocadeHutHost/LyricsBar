@echo off
setlocal
set "MSVC=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231"
set "SDK=C:\Program Files (x86)\Windows Kits\10"
set "SDKVER=10.0.26100.0"
if not exist "%MSVC%\bin\Hostx64\x64\cl.exe" (
  echo [ERROR] MSVC cl.exe not found
  exit /b 1
)
set "INCLUDE=%MSVC%\include;%SDK%\Include\%SDKVER%\um;%SDK%\Include\%SDKVER%\shared;%SDK%\Include\%SDKVER%\winrt;%SDK%\Include\%SDKVER%\cppwinrt;%SDK%\Include\%SDKVER%\ucrt"
set "LIB=%MSVC%\lib\x64;%SDK%\Lib\%SDKVER%\um\x64;%SDK%\Lib\%SDKVER%\ucrt\x64"
cd /d "%~dp0"
"%MSVC%\bin\Hostx64\x64\cl.exe" -nologo -O1 -MT -EHsc -utf-8 LyricFetcher.cpp -Fe:LyricFetcher.exe
if errorlevel 1 goto fail
"%SDK%\bin\%SDKVER%\x64\rc.exe" -nologo -fo resource.res resource.rc
if errorlevel 1 goto fail
"%MSVC%\bin\Hostx64\x64\cl.exe" -nologo -O1 -MT -W3 -EHsc -utf-8 -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -Iinc src\common.cpp src\draw.cpp src\audio.cpp src\lyrics.cpp src\playlist.cpp src\cover.cpp src\tray.cpp src\folderpick.cpp src\library.cpp src\ui.cpp src\app.cpp resource.res -Fe:LyricsBar.exe
if errorlevel 1 goto fail
del /q LyricFetcher.exe resource.res *.obj >nul 2>&1
echo [BUILD OK] LyricsBar.exe
endlocal
exit /b 0
:fail
echo [BUILD FAILED]
endlocal
exit /b 1
