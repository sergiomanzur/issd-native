@asar 1.81

; Modify these as needed
lorom						; The memory map of the ROM. Change this if the ROM uses a different memory map, or else the output may be wrong.
!ROMOffset = $87ABAA				; The ROM offset to begin disassembly from.
!DoTwoPassesFlag = 1				; If 1, the script will run twice, with the purpose of generating labels that appear before the branch that points to it. Turning this on may slow down disassembly speed, however.
!MaxBytes = 1000				; The maximum amount of bytes that will be read at a time. Setting this lower/higher will speed up/slow down disassembly.
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
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp00_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op1()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp01_UnknownOpcode()"
endif
endmacro

macro Op2()
	%Op171()
endmacro

macro Op3()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp03_UnknownOpcode()"
endif
endmacro

macro Op4()
	%Op171()
endmacro

macro Op5()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp05_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op6()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp06_UnknownOpcode()"
endif
endmacro

macro Op7()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp07_UnknownOpcode(DATA_87",hex(!Input1, 4),")"
endif
endmacro

macro Op8()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp08_UnknownOpcode()"
endif
endmacro

macro Op9()
	%readword(Input1)
	!Input2 #= !Input1+($87<<16)
	%HandleJump(!Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp09_UnknownOpcode(DATA_87",hex(!Input1, 4),")"
endif
endmacro

macro Op10()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp0A_UnknownOpcode()"
endif
endmacro

macro Op11()
	%readword(Input1)
	!Input2 #= !Input1+($87<<16)
	%HandleJump(!Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp0B_UnknownOpcode(DATA_87",hex(!Input1, 4),")"
endif
endmacro

macro Op12()
	%Op171()
endmacro

macro Op13()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp0D_UnknownOpcode()"
endif
endmacro

macro Op14()
	%readword(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp0E_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op15()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp0F_UnknownOpcode()"
endif
endmacro

macro Op16()
	%readword(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp10_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op17()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp11_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op18()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp12_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op19()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp13_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op20()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp14_UnknownOpcode()"
endif
endmacro

macro Op21()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp15_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op22()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp16_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op23()
	%Op171()
endmacro

macro Op24()
	%Op171()
endmacro

macro Op25()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp19_UnknownOpcode()"
endif
endmacro

macro Op26()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp1A_UnknownOpcode()"
endif
endmacro

macro Op27()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp1B_UnknownOpcode()"
endif
endmacro

macro Op28()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp1C_UnknownOpcode()"
endif
endmacro

macro Op29()
	%Op171()
endmacro

macro Op30()
	%Op171()
endmacro

macro Op31()
	%readbyte(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp1F_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),", $",hex(!Input3, 2),")"
endif
endmacro

macro Op32()
	%readword(Input1)
	!Input2 #= !Input1+($87<<16)
	%HandleJump(!Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp20_UnknownOpcode(DATA_87",hex(!Input1, 4),")"
endif
	%DefineLabelAfterNoPassOpcode(!CurrentOffset)
endmacro

macro Op33()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp21_UnknownOpcode()"
endif
endmacro

macro Op34()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp22_UnknownOpcode()"
endif
endmacro

macro Op35()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp23_UnknownOpcode()"
endif
endmacro

macro Op36()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp24_UnknownOpcode()"
endif
endmacro

macro Op37()
	%Op171()
endmacro

macro Op38()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp26_UnknownOpcode()"
endif
endmacro

macro Op39()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp27_UnknownOpcode()"
endif
endmacro

macro Op40()
	%Op171()
endmacro

macro Op41()
	%Op171()
endmacro

macro Op42()
	%Op171()
endmacro

macro Op43()
	%Op171()
endmacro

macro Op44()
	%Op171()
endmacro

macro Op45()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp2D_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op46()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp2E_UnknownOpcode()"
endif
endmacro

macro Op47()
	%Op171()
endmacro

macro Op48()
	%Op171()
endmacro

macro Op49()
	%Op171()
endmacro

macro Op50()
	%Op171()
endmacro

macro Op51()
	%Op171()
endmacro

macro Op52()
	%Op171()
endmacro

macro Op53()
	%Op171()
endmacro

macro Op54()
	%Op171()
endmacro

macro Op55()
	%Op171()
endmacro

macro Op56()
	%Op171()
endmacro

macro Op57()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp39_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op58()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp3A_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op59()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp3B_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op60()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp3C_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op61()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp3D_UnknownOpcode()"
endif
endmacro

macro Op62()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp3E_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op63()
	%Op171()
endmacro

macro Op64()
	%Op171()
endmacro

macro Op65()
	%Op171()
endmacro

macro Op66()
	%Op171()
endmacro

macro Op67()
	%Op171()
endmacro

macro Op68()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp44_UnknownOpcode(CODE_86",hex(!Input1, 4),")"
endif
endmacro

macro Op69()
	%Op171()
endmacro

macro Op70()
	%Op171()
endmacro

macro Op71()
	%Op171()
endmacro

macro Op72()
	%readword(Input1)
	!Input2 #= !Input1+($87<<16)
	%HandleJump(!Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp48_UnknownOpcode(DATA_87",hex(!Input1, 4),")"
endif
endmacro

macro Op73()
	%readword(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp49_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op74()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp4A_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op75()
	%Op171()
endmacro

macro Op76()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp4C_UnknownOpcode()"
endif
	%DefineLabelAfterNoPassOpcode(!CurrentOffset)
endmacro

macro Op77()
	%readword(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp4D_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op78()
	%Op171()
endmacro

macro Op79()
	%Op171()
endmacro

macro Op80()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp50_UnknownOpcode()"
endif
endmacro

macro Op81()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp51_UnknownOpcode()"
endif
endmacro

macro Op82()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp52_UnknownOpcode()"
endif
endmacro

macro Op83()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp53_UnknownOpcode()"
endif
endmacro

macro Op84()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp54_UnknownOpcode()"
endif
endmacro

macro Op85()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp55_UnknownOpcode()"
endif
endmacro

macro Op86()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp56_UnknownOpcode()"
endif
endmacro

macro Op87()
	%readbyte(Input1)
	%readword(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp57_UnknownOpcode($",hex(!Input1, 2),", DATA_87",hex(!Input2, 4),")"
endif
endmacro

macro Op88()
	%readbyte(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp58_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op89()
	%readword(Input1)
	%readword(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp59_UnknownOpcode(CODE_86",hex(!Input1, 4),", DATA_87",hex(!Input2, 4),")"
endif
endmacro

macro Op90()
	%readword(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp5A_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op91()
	%readword(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp5B_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op92()
	%Op171()
endmacro

macro Op93()
	%Op171()
endmacro

macro Op94()
	%Op171()
endmacro

macro Op95()
	%Op171()
endmacro

macro Op96()
	%Op171()
endmacro

macro Op97()
	%Op171()
endmacro

macro Op98()
	%Op171()
endmacro

macro Op99()
	%Op171()
endmacro

macro Op100()
	%Op171()
endmacro

macro Op101()
	%Op171()
endmacro

macro Op102()
	%Op171()
endmacro

macro Op103()
	%Op171()
endmacro

macro Op104()
	%Op171()
endmacro

macro Op105()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp69_UnknownOpcode()"
endif
endmacro

macro Op106()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp6A_UnknownOpcode()"
endif
endmacro

macro Op107()
	%Op171()
endmacro

macro Op108()
	%Op171()
endmacro

macro Op109()
	%Op171()
endmacro

macro Op110()
	%Op171()
endmacro

macro Op111()
	%Op171()
endmacro

macro Op112()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp70_UnknownOpcode()"
endif
endmacro

macro Op113()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp71_UnknownOpcode()"
endif
endmacro

macro Op114()
	%Op171()
endmacro

macro Op115()
	%Op171()
endmacro

macro Op116()
	%Op171()
endmacro

macro Op117()
	%Op171()
endmacro

macro Op118()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp76_UnknownOpcode()"
endif
endmacro

macro Op119()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp77_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op120()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp78_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op121()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp79_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op122()
	%readbyte(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp7A_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op123()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp7B_UnknownOpcode()"
endif
endmacro

macro Op124()
	%Op171()
endmacro

macro Op125()
	%Op171()
endmacro

macro Op126()
	%Op171()
endmacro

macro Op127()
	%readword(Input1)
	%readbyte(Input2)
	!Input3 #= !Input1+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp7F_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op128()
	%readword(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp80_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op129()
	%readbyte(Input1)
	%readword(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp81_UnknownOpcode($",hex(!Input1, 2),", DATA_87",hex(!Input2, 4),")"
endif
endmacro

macro Op130()
	%readword(Input1)
	%readbyte(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp82_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
	%DefineLabelAfterNoPassOpcode(!CurrentOffset)
endmacro

macro Op131()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp83_UnknownOpcode()"
endif
endmacro

macro Op132()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp84_UnknownOpcode()"
endif
endmacro

macro Op133()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp85_UnknownOpcode()"
endif
endmacro

macro Op134()
	%readbyte(Input1)
	%readword(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp86_UnknownOpcode($",hex(!Input1, 2),", DATA_87",hex(!Input2, 4),")"
endif
endmacro

macro Op135()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp87_UnknownOpcode()"
endif
endmacro

macro Op136()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp88_UnknownOpcode()"
endif
endmacro

macro Op137()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp89_UnknownOpcode()"
endif
endmacro

macro Op138()
	%readword(Input1)
	%readword(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp8A_UnknownOpcode(CODE_86",hex(!Input1, 4),", DATA_87",hex(!Input2, 4),")"
endif
endmacro

macro Op139()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp8B_UnknownOpcode()"
endif
endmacro

macro Op140()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp8C_UnknownOpcode()"
endif
endmacro

macro Op141()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp8D_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op142()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp8E_UnknownOpcode()"
endif
endmacro

macro Op143()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp8F_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op144()
	%readbyte(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp90_UnknownOpcode($",hex(!Input1, 2),")"
endif
endmacro

macro Op145()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp91_UnknownOpcode()"
endif
endmacro

macro Op146()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp92_UnknownOpcode()"
endif
endmacro

macro Op147()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp93_UnknownOpcode()"
endif
endmacro

macro Op148()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp94_UnknownOpcode()"
endif
endmacro

macro Op149()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp95_UnknownOpcode()"
endif
endmacro

macro Op150()
	%readword(Input1)
	!Input2 #= !Input1+($87<<16)
	%HandleJump(!Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp96_UnknownOpcode(DATA_87",hex(!Input1, 4),")"
endif
endmacro

macro Op151()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp97_UnknownOpcode()"
endif
endmacro

macro Op152()
	%readbyte(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp98_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op153()
	%readbyte(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp99_UnknownOpcode($",hex(!Input1, 2),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op154()
	%readword(Input1)
	%readbyte(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp9A_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op155()
	%readword(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp9B_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op156()
	%readword(Input1)
	%readword(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp9C_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 4),")"
endif
endmacro

macro Op157()
	%readword(Input1)
	%readbyte(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp9D_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op158()
	%readword(Input1)
	%readbyte(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOp9E_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op159()
if !Pass == 1
	print "	%ISSD_UnkMScriptOp9F_UnknownOpcode()"
endif
endmacro

macro Op160()
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA0_UnknownOpcode()"
endif
endmacro

macro Op161()
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA1_UnknownOpcode()"
endif
endmacro

macro Op162()
	%readword(Input1)
	%readbyte(Input2)
	!Input3 #= !Input2+($87<<16)
	%HandleJump(!Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA2_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op163()
	%readword(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA3_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op164()
	%readword(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA4_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", $",hex(!Input3, 2),")"
endif
endmacro

macro Op165()
	%readword(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA5_UnknownOpcode($",hex(!Input1, 4),", $",hex(!Input2, 2),", $",hex(!Input3, 2),")"
endif
endmacro

macro Op166()
	%readword(Input1)
	%readbyte(Input2)
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA6_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),")"
endif
endmacro

macro Op167()
	%readword(Input1)
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA7_UnknownOpcode($",hex(!Input1, 4),")"
endif
endmacro

macro Op168()
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA8_UnknownOpcode()"
endif
endmacro

macro Op169()
if !Pass == 1
	print "	%ISSD_UnkMScriptOpA9_UnknownOpcode()"
endif
endmacro

macro Op170()
	%readword(Input1)
	%readbyte(Input2)
	%readbyte(Input3)
	!Input4 #= !Input3+($87<<16)
	%HandleJump(!Input4)
if !Pass == 1
	print "	%ISSD_UnkMScriptOpAA_UnknownOpcode(DATA_87",hex(!Input1, 4),", $",hex(!Input2, 2),", $",hex(!Input3, 2),")"
endif
endmacro

macro Op171()
if !Pass == 1
	print "	Invalid opcode: $",hex(!Input1, 2)
endif
endmacro

org !ROMOffset
if !DoTwoPassesFlag == 1
	while !ByteCounter < !MaxBytes
		%GetOpcode()
		if !Input1 > 170
			%Op171()
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
	if !Input1 > 170
		%Op171()
	else
		%Op!Input1()
	endif
	!LoopCounter #= !LoopCounter+1
endif

!Input1 #= !ROMOffset+!ByteCounter
print "Disassembly has ended at $",hex(!ROMOffset+!ByteCounter, 6)
