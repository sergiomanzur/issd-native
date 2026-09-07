@echo off
setlocal EnableDelayedExpansion

set PATH="../../Global"

asar.exe DisassemblePaletteAnimationScript.asm ISSD.sfc > output1.asm

pause
exit