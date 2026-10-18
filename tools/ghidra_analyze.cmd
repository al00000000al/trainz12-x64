@echo off
rem Import and auto-analyse modules into the Ghidra project, then export function stats.
rem usage: ghidra_analyze.cmd <log file> <binary> [binary ...]
set "JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot"
set "GHIDRA_HEADLESS_MAXMEM=12G"
set "LOG=%~1"
shift
set "FILES="
:collect
if "%~1"=="" goto run
set FILES=%FILES% "%~1"
shift
goto collect
:run
call "D:\tools\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" D:\trainz12-decomp\project trainz12 -import %FILES% -scriptPath "D:\universal modder\trainz12-x64\ghidra_scripts" -postScript ExportFunctionStats.java D:/trainz12-decomp/out -max-cpu 5 -analysisTimeoutPerFile 14400 > "%LOG%" 2>&1 < NUL
