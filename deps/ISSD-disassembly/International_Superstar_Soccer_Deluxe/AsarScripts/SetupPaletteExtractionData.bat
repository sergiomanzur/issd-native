@echo off
setlocal EnableDelayedExpansion

set PATH="../../Global"

asar.exe SetupPaletteExtractionData.asm ISSD.sfc > output1.asm

pause
asar.exe output1.asm ISSD.sfc > output2.asm

pause
exit