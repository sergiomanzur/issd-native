@asar 1.81

; Modify these as needed
lorom						; The memory map of the ROM. Change this if the ROM uses a different memory map, or else the output may be wrong.
!ROMOffset = $81852B				; The ROM offset to begin disassembly from.
!DoTwoPassesFlag = 1				; If 1, the script will run twice, with the purpose of generating labels that appear before the branch that points to it. Turning this on may slow down disassembly speed, however.
!MaxBytes = 3135				; The maximum amount of bytes that will be read at a time. Setting this lower/higher will speed up/slow down disassembly.
!Bank = 81					; Affects the bank byte for the label used in JSR/JMP instructions.

; Don't touch these
!Input1 = $00
!Input2 = $00
!Output = ""
!ByteCounter = 0
!LoopCounter = 0
!Pass = 0
!CurrentOffset = 0

macro GetOpcode()
	!Input1 #= read1(!ROMOffset+!ByteCounter)
	;!Input1 #= !LoopCounter
	!ByteCounter #= !ByteCounter+1
	!CurrentOffset #= !ROMOffset+!ByteCounter
endmacro

macro readbyte(Input)
	!<Input> #= read1(!ROMOffset+!ByteCounter)
	;!<Input> = $01
	!ByteCounter #= !ByteCounter+1
	!CurrentOffset #= !ROMOffset+!ByteCounter
endmacro

macro readword(Input)
	!<Input> #= read2(!ROMOffset+!ByteCounter)
	;!<Input> = $0123
	!ByteCounter #= !ByteCounter+2
	!CurrentOffset #= !ROMOffset+!ByteCounter
endmacro

macro readlong(Input)
	!<Input> #= read3(!ROMOffset+!ByteCounter)
	;!<Input> = $012345
	!ByteCounter #= !ByteCounter+3
	!CurrentOffset #= !ROMOffset+!ByteCounter
endmacro

macro PrintLabel(Address)
if defined("ROM_<Address>") == 1
	if !ROM_<Address> == 1
		print ""
	endif
	print "DATA_",hex(!ROMOffset+!ByteCounter, 6),":"
endif
endmacro

macro DefineLabelAfterNoPassOpcode(Address)
	!ROM_<Address> = 1
endmacro

macro HandleJump(Address)
if defined("ROM_<Address>") == 0
	!ROM_<Address> = 0
endif
endmacro

macro Op0()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp00_UnknownOpcode()"
endif
endmacro

macro Op1()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp01_UnknownOpcode()"
endif
endmacro

macro Op2()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp02_UnknownOpcode()"
endif
endmacro

macro Op3()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp03_UnknownOpcode()"
endif
endmacro

macro Op4()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp04_UnknownOpcode()"
endif
endmacro

macro Op5()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp05_UnknownOpcode()"
endif
endmacro

macro Op6()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp06_UnknownOpcode()"
endif
endmacro

macro Op7()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp07_UnknownOpcode()"
endif
endmacro

macro Op8()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp08_UnknownOpcode()"
endif
endmacro

macro Op9()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp09_UnknownOpcode()"
endif
endmacro

macro Op10()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp0A_UnknownOpcode()"
endif
endmacro

macro Op11()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp0B_UnknownOpcode()"
endif
endmacro

macro Op12()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp0C_UnknownOpcode()"
endif
endmacro

macro Op13()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp0D_UnknownOpcode()"
endif
endmacro

macro Op14()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp0E_UnknownOpcode()"
endif
endmacro

macro Op15()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp0F_UnknownOpcode()"
endif
endmacro

macro Op16()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp10_UnknownOpcode()"
endif
endmacro

macro Op17()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp11_UnknownOpcode()"
endif
endmacro

macro Op18()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp12_UnknownOpcode()"
endif
	%DefineLabelAfterNoPassOpcode(!CurrentOffset)
endmacro

macro Op19()
	%readword(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
	%readbyte(Input4)
	%readlong(Input5)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp13_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", $",hex(!Input3, 2),", $",hex(!Input4, 2),", DATA_",hex(!Input5, 6),")"
endif
endmacro

macro Op20()
	%readword(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
	%readbyte(Input4)
	%readlong(Input5)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp14_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", $",hex(!Input3, 2),", $",hex(!Input4, 2),", DATA_",hex(!Input5, 6),")"
endif
endmacro

macro Op21()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp15_UnknownOpcode()"
endif
endmacro

macro Op22()
	%readword(Input1)
	!Input2 #= !Input1+($81<<16)
	%HandleJump(!Input2)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp16_UnknownOpcode(DATA_81",hex(!Input1, 4),")"
endif
	%DefineLabelAfterNoPassOpcode(!CurrentOffset)
endmacro

macro Op23()
	%readword(Input1)
	!Input2 #= !Input1+($81<<16)
	%HandleJump(!Input2)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp17_UnknownOpcode(DATA_81",hex(!Input1, 4),")"
endif
endmacro

macro Op24()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp18_UnknownOpcode()"
endif
endmacro

macro Op25()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp19_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op26()
	%readword(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
	%readbyte(Input4)
	%readbyte(Input5)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp1A_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", $",hex(!Input3, 2),", $",hex(!Input4, 2),", $",hex(!Input5, 2),")"
endif
endmacro

macro Op27()
	%readword(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
if !Input3 == 0
	if !Pass == 1
		print "	%ISSD_PalAnimScriptOp1B_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", .End)"
	endif
elseif !Input3 == 1
	%readbyte(Input3)
	%readbyte(Input4)
	if !Pass == 1
		print "	%ISSD_PalAnimScriptOp1B_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", .End, $",hex(!Input3, 2),", $",hex(!Input4, 2),")"
	endif
elseif !Input3 == 2
	%readbyte(Input3)
	%readbyte(Input4)
	%readbyte(Input5)
	%readbyte(Input6)
	if !Pass == 1
		print "	%ISSD_PalAnimScriptOp1B_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", .End, $",hex(!Input3, 2),", $",hex(!Input4, 2),", $",hex(!Input5, 2),", $",hex(!Input6, 2),")"
	endif
elseif !Input3 == 3
	%readbyte(Input3)
	%readbyte(Input4)
	%readbyte(Input5)
	%readbyte(Input6)
	%readbyte(Input7)
	%readbyte(Input8)
	if !Pass == 1
		print "	%ISSD_PalAnimScriptOp1B_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", .End, $",hex(!Input3, 2),", $",hex(!Input4, 2),", $",hex(!Input5, 2),", $",hex(!Input6, 2),", $",hex(!Input7, 2),", $",hex(!Input8, 2),")"
	endif
endif
endmacro

macro Op28()
	%readword(Input1)
	%readbyte(Input2)
	%readword(Input3)
	%readbyte(Input4)
	%readbyte(Input5)
	%readbyte(Input6)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp1C_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", $",hex(!Input3, 2),", $",hex(!Input4, 2),", $",hex(!Input5, 2),", $",hex(!Input6, 2),")"
endif
endmacro

macro Op29()
	%readword(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
	%readbyte(Input4)
	%readbyte(Input5)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp1D_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", $",hex(!Input3, 2),", $",hex(!Input4, 2),", $",hex(!Input5, 2),")"
endif
endmacro

macro Op30()
	%readword(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
if !Input3 == 0
	if !Pass == 1
		print "	%ISSD_PalAnimScriptOp1E_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", .End)"
	endif
elseif !Input3 == 1
	%readbyte(Input3)
	%readbyte(Input4)
	if !Pass == 1
		print "	%ISSD_PalAnimScriptOp1E_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", .End, $",hex(!Input3, 2),", $",hex(!Input4, 2),")"
	endif
elseif !Input3 == 2
	%readbyte(Input3)
	%readbyte(Input4)
	%readbyte(Input5)
	%readbyte(Input6)
	if !Pass == 1
		print "	%ISSD_PalAnimScriptOp1E_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", .End, $",hex(!Input3, 2),", $",hex(!Input4, 2),", $",hex(!Input5, 2),", $",hex(!Input6, 2),")"
	endif
elseif !Input3 == 3
	%readbyte(Input3)
	%readbyte(Input4)
	%readbyte(Input5)
	%readbyte(Input6)
	%readbyte(Input7)
	%readbyte(Input8)
	if !Pass == 1
		print "	%ISSD_PalAnimScriptOp1E_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", .End, $",hex(!Input3, 2),", $",hex(!Input4, 2),", $",hex(!Input5, 2),", $",hex(!Input6, 2),", $",hex(!Input7, 2),", $",hex(!Input8, 2),")"
	endif
endif
endmacro

macro Op31()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp1F_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op32()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp20_UnknownOpcode()"
endif
endmacro

macro Op33()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp21_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op34()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp22_UnknownOpcode()"
endif
	%DefineLabelAfterNoPassOpcode(!CurrentOffset)
endmacro

macro Op35()
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp23_UnknownOpcode()"
endif
endmacro

macro Op36()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp24_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op37()
	%readword(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp25_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op38()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp26_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op39()
	%readword(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp27_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op40()
	%readword(Input1)
	%readbyte(Input2)
	%readlong(Input3)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp28_BufferPalette($",hex(!Input1, 4),", $",hex(!Input2, 2),", DATA_",hex(!Input3, 6),")"
endif
endmacro

macro Op41()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp29_UnknownOpcode($7E",hex(!Input1, 4),")"
endif
endmacro

macro Op42()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp2A_BufferMultiplePalettes(.End)"
endif
if !Input1 != 0
	!LoopCounter #= 0
	while !LoopCounter < !Input1
	%readbyte(Input2)
	%readlong(Input3)
	if !Pass == 1
		print "		%ISSD_PalUploadOffsets($",hex(!Input2, 2),", DATA_",hex(!Input3, 6),")"
	endif
	!LoopCounter #= !LoopCounter+1
	endif
	if !Pass == 1
		print "	.End:"
	endif
endif
endmacro

macro Op43()
	%readbyte(Input1)
	%readlong(Input2)
if !Pass == 1
	print "	%ISSD_PalAnimScriptOp2B_UnknownOpcode($",hex(!Input1, 2),", DATA_",hex(!Input2, 6),")"
endif
endmacro

macro Op44()
if !Pass == 1
	print "	Invalid opcode: $",hex(!Input1, 2)
endif
endmacro

org !ROMOffset
if !DoTwoPassesFlag == 1
	while !ByteCounter < !MaxBytes
		%GetOpcode()
		if !Input1 > 43
			%Op44()
		else
			%Op!Input1()
		endif
		!LoopCounter #= !LoopCounter+1
	endif
	!LoopCounter #= 0
	!ByteCounter #= 0
endif
	!Pass = 1
while !ByteCounter < !MaxBytes
	%PrintLabel(!CurrentOffset)
	%GetOpcode()
	if !Input1 > 43
		%Op44()
	else
		%Op!Input1()
	endif
	!LoopCounter #= !LoopCounter+1
endif

!Input1 #= !ROMOffset+!ByteCounter
print "Disassembly has ended at $",hex(!ROMOffset+!ByteCounter, 6)
