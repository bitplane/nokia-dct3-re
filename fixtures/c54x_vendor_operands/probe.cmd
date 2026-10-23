@echo off
set TEMP=Z:\mnt\work\tmp
set TMP=Z:\mnt\work\tmp
set PATH=Z:\mnt\work\bin;%PATH%
Z:\mnt\work\bin\cl500.exe -v548 --asm_listing --compile_only operand_probe.asm
exit /b %ERRORLEVEL%
