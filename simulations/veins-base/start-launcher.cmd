@echo off
REM ========================================
REM TriBFT-Veins SUMO Launcher - Erlangen
REM ========================================

set OMNETPP=D:\environment\OMNeT++6.2\omnetpp-6.2.0
set VEINS=D:\environment\veins5.3.1\veins-veins-5.3.1
set SUMO_BIN=D:\environment\sumo-1.21.0\bin\sumo-gui.exe

REM Include clang64/bin for Python and Qt DLLs
set PATH=%OMNETPP%\tools\win32.x86_64\clang64\bin;%OMNETPP%\tools\win32.x86_64\usr\bin;%PATH%

echo.
echo ========================================
echo TriBFT-Veins SUMO Launcher - Erlangen
echo ========================================
echo.
echo Road Network: Erlangen
echo Using SUMO: erlangen.sumo.cfg
echo Listening on port 9999
echo *** Keep this window running! ***
echo.

REM Launch SUMO with veins_launchd
python "%VEINS%\bin\veins_launchd" -vv -c "%SUMO_BIN%"

pause
