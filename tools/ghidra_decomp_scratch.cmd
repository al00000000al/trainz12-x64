@echo off
rem Analyse modules in a throwaway project (the main project may be busy) and dump decompiled C.
rem usage: ghidra_decomp_scratch.cmd <log file> <binary> [binary ...]
set "JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot"
set "GHIDRA_HEADLESS_MAXMEM=4G"
set "LOG=%~1"
shift
set "FILES="
:collect
if "%~1"=="" goto run
set FILES=%FILES% "%~1"
shift
goto collect
:run
if not exist D:\trainz12-decomp\scratch mkdir D:\trainz12-decomp\scratch
call "D:\tools\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" D:\trainz12-decomp\scratch scratch -import %FILES% -overwrite -scriptPath "D:\universal modder\trainz12-x64\ghidra_scripts" -postScript DecompileAll.java D:/trainz12-decomp/decomp -max-cpu 1 > "%LOG%" 2>&1 < NUL
