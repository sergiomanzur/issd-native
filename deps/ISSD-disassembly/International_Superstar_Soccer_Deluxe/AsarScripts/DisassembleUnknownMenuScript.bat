@echo off
setlocal EnableDelayedExpansion

set PATH="../../Global"

asar.exe DisassembleUnknownMenuScript.asm ISSD.sfc > output1.asm

pause
exit