@echo off
setlocal EnableDelayedExpansion

set PATH="../../Global"

asar.exe DisassembleMenuInitScript.asm ISSD.sfc > output1.asm

pause
exit