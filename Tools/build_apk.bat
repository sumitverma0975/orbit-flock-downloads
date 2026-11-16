@echo off
setlocal
rem Requires Python 3, UE 5.5 Android support, SDK/NDK and the missing game assets.
rem Usage: set UE_ROOT=C:\Program Files\Epic Games\UE_5.5 then Tools\build_apk.bat
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.5"
set "PROJ=%~dp0..\DelhiDrive.uproject"
set "OUT=%~dp0..\Artifacts\Android-%RANDOM%-%RANDOM%"
if not "%DELHIDRIVE_BUILD_OUTPUT%"=="" set "OUT=%DELHIDRIVE_BUILD_OUTPUT%"

python "%~dp0setup_map.py" --ue-root "%UE_ROOT%"
if errorlevel 1 exit /b 1
python "%~dp0preflight.py" --ue-root "%UE_ROOT%"
if errorlevel 1 exit /b 1
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" Turnkey -command=VerifySdk -platform=Android -unattended
if errorlevel 1 exit /b 1

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJ%" -noP4 -utf8output ^
  -platform=Android -cookflavor=ASTC -clientconfig=Shipping -unattended ^
  -build -cook -stage -pak -compressed -package -archive -archivedirectory="%OUT%"
if errorlevel 1 exit /b 1

python "%~dp0preflight.py" --check-apks "%OUT%"
if errorlevel 1 exit /b 1
exit /b 0
