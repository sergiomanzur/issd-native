@asar 1.81

; Modify these as needed
lorom						; The memory map of the ROM. Change this if the ROM uses a different memory map, or else the output may be wrong.
!ROMOffset = $87B5D3				; The ROM offset to begin disassembly from.
!DoTwoPassesFlag = 1				; If 1, the script will run twice, with the purpose of generating labels that appear before the branch that points to it. Turning this on may slow down disassembly speed, however.
!MaxBytes = 3135				; The maximum amount of bytes that will be read at a time. Setting this lower/higher will speed up/slow down disassembly.
!Bank = 87					; Affects the bank byte for the label used in JSR/JMP instructions.

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
	print "	%ISSD_MIScriptOp00_UnknownOpcode()"
endif
endmacro

macro Op2()
if !Pass == 1
	print "	%ISSD_MIScriptOp02_UnknownOpcode()"
endif
endmacro

macro Op4()
if !Pass == 1
	print "	%ISSD_MIScriptOp04_UnknownOpcode()"
endif
endmacro

macro Op6()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp06_UnknownOpcode(DATA_82",hex(!Input1, 4),")"
endif
endmacro

macro Op8()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp08_UnknownOpcode(DATA_82",hex(!Input1, 4),")"
endif
endmacro

macro Op10()
	%readword(Input1)
	%readword(Input2)
	%readbyte(Input3)
	%readword(Input4)
if !Pass == 1
	print "	%ISSD_MIScriptOp0A_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 2),hex(!Input4, 4),")"
endif
endmacro

macro Op12()
if !Pass == 1
	print "	%ISSD_MIScriptOp0C_UnknownOpcode()"
endif
endmacro

macro Op14()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp0E_UnknownOpcode(DATA_81",hex(!Input1, 4),")"
endif
endmacro

macro Op16()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp10_UnknownOpcode(DATA_81",hex(!Input1, 4),")"
endif
endmacro

macro Op18()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp12_UnknownOpcode(DATA_81",hex(!Input1, 4),")"
endif
endmacro

macro Op20()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp14_UnknownOpcode(DATA_81",hex(!Input1, 4),")"
endif
endmacro

macro Op22()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp16_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op24()
	%readword(Input1)
	%readword(Input2)
	%readword(Input3)
if !Pass == 1
	print "	%ISSD_MIScriptOp18_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 4),")"
endif
endmacro

macro Op26()
	%readword(Input1)
	%readword(Input2)
	%readword(Input3)
	%readbyte(Input4)
if !Pass == 1
	print "	%ISSD_MIScriptOp1A_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 4),", $",hex(!Input4, 2),")"
endif
endmacro

macro Op28()
	%readword(Input1)
	%readword(Input2)
	%readbyte(Input3)
if !Pass == 1
	print "	%ISSD_MIScriptOp1C_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 2),")"
endif
endmacro

macro Op30()
if !Pass == 1
	print "	%ISSD_MIScriptOp1E_UnknownOpcode()"
endif
endmacro

macro Op32()
if !Pass == 1
	print "	%ISSD_MIScriptOp20_UnknownOpcode()"
endif
endmacro

macro Op34()
	%readword(Input1)
	%readword(Input2)
	%readword(Input3)
if !Pass == 1
	print "	%ISSD_MIScriptOp22_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 4),")"
endif
endmacro

macro Op36()
	%readword(Input1)
	%readword(Input2)
	%readword(Input3)
	%readbyte(Input4)
if !Pass == 1
	print "	%ISSD_MIScriptOp24_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 4),", $",hex(!Input4, 2),")"
endif
endmacro

macro Op38()
	%readword(Input1)
	%readword(Input2)
	%readbyte(Input3)
if !Pass == 1
	print "	%ISSD_MIScriptOp26_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 2),")"
endif
endmacro

macro Op40()
if !Pass == 1
	print "	%ISSD_MIScriptOp28_UnknownOpcode()"
endif
endmacro

macro Op42()
if !Pass == 1
	print "	%ISSD_MIScriptOp2A_UnknownOpcode()"
endif
endmacro

macro Op44()
if !Pass == 1
	print "	%ISSD_MIScriptOp2C_UnknownOpcode()"
endif
endmacro

macro Op46()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp2E_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op48()
if !Pass == 1
	print "	%ISSD_MIScriptOp30_UnknownOpcode()"
endif
endmacro

macro Op50()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp32_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op52()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp34_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op54()
if !Pass == 1
	print "	%ISSD_MIScriptOp36_UnknownOpcode()"
endif
endmacro

macro Op56()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp38_UnknownOpcode(DATA_87",hex(!Input1, 4),")"
endif
endmacro

macro Op58()
	%readword(Input1)
	%readword(Input2)
	%readbyte(Input3)
if !Pass == 1
	print "	%ISSD_MIScriptOp3A_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 2),")"
endif
endmacro

macro Op60()
	%readword(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp3C_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op62()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp3E_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op64()
	%readword(Input1)
	%readword(Input2)
	%readword(Input3)
if !Pass == 1
	print "	%ISSD_MIScriptOp40_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 4),")"
endif
endmacro

macro Op66()
	%readword(Input1)
	%readword(Input2)
	%readword(Input3)
	%readbyte(Input4)
if !Pass == 1
	print "	%ISSD_MIScriptOp42_UnknownOpcode($",hex(!Input1, 4),", DATA_87",hex(!Input2, 4),", $",hex(!Input3, 4),", $",hex(!Input4, 2),")"
endif
endmacro

macro Op68()
	%readword(Input1)
	%readword(Input2)
	%readbyte(Input3)
if !Pass == 1
	print "	%ISSD_MIScriptOp44_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 2),")"
endif
endmacro

macro Op70()
if !Pass == 1
	print "	%ISSD_MIScriptOp46_UnknownOpcode()"
endif
endmacro

macro Op72()
if !Pass == 1
	print "	%ISSD_MIScriptOp48_UnknownOpcode()"
endif
endmacro

macro Op74()
if !Pass == 1
	print "	%ISSD_MIScriptOp4A_UnknownOpcode()"
endif
endmacro

macro Op76()
if !Pass == 1
	print "	%ISSD_MIScriptOp4C_UnknownOpcode()"
endif
endmacro

macro Op78()
	%readword(Input1)
	!Input2 #= !Input1+($87<<16)
	%HandleJump(!Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp4E_UnknownOpcode(DATA_87",hex(!Input1, 4),")"
endif
	%DefineLabelAfterNoPassOpcode(!CurrentOffset)
endmacro

macro Op80()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp50_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op82()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp52_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op84()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp54_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op86()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp56_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op88()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp58_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op90()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp5A_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op92()
if !Pass == 1
	print "	%ISSD_MIScriptOp5C_UnknownOpcode()"
endif
endmacro

macro Op94()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp5E_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op96()
if !Pass == 1
	print "	%ISSD_MIScriptOp60_UnknownOpcode()"
endif
endmacro

macro Op98()
if !Pass == 1
	print "	%ISSD_MIScriptOp62_UnknownOpcode()"
endif
endmacro

macro Op100()
	%readword(Input1)
	%readword(Input2)
	%readbyte(Input3)
if !Pass == 1
	print "	%ISSD_MIScriptOp64_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 2),")"
endif
endmacro

macro Op102()
if !Pass == 1
	print "	%ISSD_MIScriptOp66_UnknownOpcode()"
endif
endmacro

macro Op104()
	%readword(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp68_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op106()
	%readword(Input1)
	%readword(Input2)
	%readbyte(Input3)
	%readword(Input4)
	%readword(Input5)
if !Pass == 1
	print "	%ISSD_MIScriptOp6A_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 2),", $",hex(!Input4, 4),", $",hex(!Input5, 4),")"
endif
endmacro

macro Op108()
	%readword(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_MIScriptOp6C_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op110()
	%readword(Input1)
	%readword(Input2)
	%readword(Input3)
	%readbyte(Input4)
if !Pass == 1
	print "	%ISSD_MIScriptOp6E_UnknownOpcode($",hex(!Input1, 4),", DATA_87",hex(!Input2, 4),", $",hex(!Input3, 4),", $",hex(!Input4, 2),")"
endif
endmacro

macro Op112()
	%readword(Input1)
	%readword(Input2)
	%readword(Input3)
	%readword(Input4)
	%readbyte(Input5)
if !Pass == 1
	print "	%ISSD_MIScriptOp70_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 4),", $",hex(!Input4, 4),", $",hex(!Input5, 2),")"
endif
endmacro

macro Op114()
	%readword(Input1)
	%readword(Input2)
	%readword(Input3)
	%readbyte(Input4)
if !Pass == 1
	print "	%ISSD_MIScriptOp72_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),", $",hex(!Input3, 4),", $",hex(!Input4, 2),")"
endif
endmacro

macro Op116()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp74_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op118()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_MIScriptOp76_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op120()
	%readbyte(Input1)
	%readbyte(Input2)
	%readword(Input3)
if !Pass == 1
	print "	%ISSD_MIScriptOp78_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),", $",hex(!Input3, 4),")"
endif
endmacro

macro Op121()
if !Pass == 1
	print "	Invalid opcode: $",hex(!Input1, 2)
endif
endmacro

macro Op255()
if !Pass == 1
	print "	%ISSD_MIScriptOpFF_UnknownOpcode()"
endif
	%DefineLabelAfterNoPassOpcode(!CurrentOffset)
endmacro

org !ROMOffset
if !DoTwoPassesFlag == 1
	while !ByteCounter < !MaxBytes
		%GetOpcode()
		if !Input1 == 255
			%Op255()
		elseif !Input1&$01 == 1
			%Op121()
		elseif !Input1 > 120
			%Op121()
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
	if !Input1 == 255
		%Op255()
	elseif !Input1&$01 == 1
		%Op121()
	elseif !Input1 > 120
		%Op121()
	else
		%Op!Input1()
	endif
	!LoopCounter #= !LoopCounter+1
endif

!Input1 #= !ROMOffset+!ByteCounter
print "Disassembly has ended at $",hex(!ROMOffset+!ByteCounter, 6)
