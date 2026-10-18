@echo off
rem Run a script over an already analysed program in the main project, without re-analysis.
rem usage: ghidra_run.cmd <log file> <program name> <script> [script args...]
set "JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot"
set "GHIDRA_HEADLESS_MAXMEM=12G"
set "LOG=%~1"
set "PROG=%~2"
set "SCRIPT=%~3"
shift
shift
shift
set "ARGS="
:collect
if "%~1"=="" goto run
set ARGS=%ARGS% "%~1"
shift
goto collect
:run
call "D:\tools\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" D:\trainz12-decomp\project trainz12 -process "%PROG%" -noanalysis -scriptPath "D:\universal modder\trainz12-x64\ghidra_scripts" -postScript %SCRIPT% %ARGS% > "%LOG%" 2>&1 < NUL
