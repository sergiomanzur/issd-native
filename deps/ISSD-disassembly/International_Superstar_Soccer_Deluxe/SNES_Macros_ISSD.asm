
;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp00_UnknownOpcode()
	db $00
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp01_UnknownOpcode()
	db $01
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp02_UnknownOpcode()
	db $02
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp03_UnknownOpcode()
	db $03
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp04_UnknownOpcode()
	db $04
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp05_UnknownOpcode()
	db $05
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp06_UnknownOpcode()
	db $06
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp07_UnknownOpcode()
	db $07
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp08_UnknownOpcode()
	db $08
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp09_UnknownOpcode()
	db $09
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp0A_UnknownOpcode()
	db $0A
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp0B_UnknownOpcode()
	db $0B
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp0C_UnknownOpcode()
	db $0C
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp0D_UnknownOpcode()
	db $0D
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp0E_UnknownOpcode()
	db $0E
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp0F_UnknownOpcode()
	db $0F
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp10_UnknownOpcode()
	db $10
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp11_UnknownOpcode()
	db $11
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp12_UnknownOpcode()
	db $12
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp13_UnknownOpcode(Param1, Param2, Param3, Param4, Param5)
	db $13 : dw <Param1> : db <Param2>,<Param3>,<Param4> : dl <Param5>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp14_UnknownOpcode(Param1, Param2, Param3, Param4, Param5)
	db $14 : dw <Param1> : db <Param2>,<Param3>,<Param4> : dl <Param5>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp15_UnknownOpcode()
	db $15
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp16_UnknownOpcode(Param1)
	db $16 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp17_UnknownOpcode(Param1)
	db $17 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp18_UnknownOpcode()
	db $18
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp19_UnknownOpcode(Param1, Param2)
	db $19 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp1A_UnknownOpcode(Param1, Param2, Param3, Param4, Param5)
	db $1A : dw <Param1> : db <Param2>,<Param3>,<Param4>,<Param5>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp1B_UnknownOpcode(Param1, Param2, Param3, ...)
	db $1B : dw <Param1> : db <Param2>,<Param3>
!LoopCounter #= 0
while !LoopCounter < sizeof(...)
	db <!LoopCounter>
	!LoopCounter #= !LoopCounter+1
endif
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp1C_UnknownOpcode(Param1, Param2, Param3, Param4, Param5, Param6)
	db $1C : dw <Param1> : db <Param2> : dw <Param3> : db <Param4>,<Param5>,<Param6>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp1D_UnknownOpcode(Param1, Param2, Param3, Param4, Param5)
	db $1D : dw <Param1> : db <Param2>,<Param3>,<Param4>,<Param5>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp1E_UnknownOpcode(Param1, Param2, Param3, ...)
	db $1E : dw <Param1> : db <Param2>,<Param3>
!LoopCounter #= 0
while !LoopCounter < sizeof(...)
	db <!LoopCounter>
	!LoopCounter #= !LoopCounter+1
endif
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp1F_UnknownOpcode(Param1)
	db $1F : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp20_UnknownOpcode()
	db $20
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp21_UnknownOpcode(Param1)
	db $21 : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp22_UnknownOpcode()
	db $22
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp23_UnknownOpcode()
	db $23
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp24_UnknownOpcode(Param1, Param2)
	db $24 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp25_UnknownOpcode(Param1, Param2)
	db $25 : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp26_UnknownOpcode()
	db $26
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp27_UnknownOpcode(Param1, Param2)
	db $27 : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp28_BufferPalette(Param1, Param2, Param3)
	db $28 : dw <Param1> : db <Param2> : dl <Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp29_UnknownOpcode(Param1)
	db $29 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp2A_BufferMultiplePalettes(Param1)
	db $2A : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadScriptOp2B_UnknownOpcode(Param1, Param2)
	db $2B : db <Param1> : dl <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_PalUploadOffsets(CGRAMIndex, Pointer)
	db <CGRAMIndex> : dl <Pointer>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp00_UnknownOpcode()
	db $00
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp02_UnknownOpcode()
	db $02
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp04_UnknownOpcode()
	db $04
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp06_UnknownOpcode(Param1)
	db $06 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp08_UnknownOpcode(Param1)
	db $08 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp0A_UnknownOpcode(Param1, Param2, Param3)
	db $0A : dw <Param1>,<Param2> : db <Param3>>>16 : dw <Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp0C_UnknownOpcode()
	db $0C
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp0E_UnknownOpcode(Param1)
	db $0E : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp10_UnknownOpcode(Param1)
	db $10 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp12_UnknownOpcode(Param1)
	db $12 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp14_UnknownOpcode(Param1)
	db $14 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp16_UnknownOpcode(Param1)
	db $16 : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp18_UnknownOpcode(Param1, Param2, Param3)
	db $18 : dw <Param1>,<Param2>,<Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp1A_UnknownOpcode(Param1, Param2, Param3, Param4)
	db $1A : dw <Param1>,<Param2>,<Param3> : db <Param4>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp1C_UnknownOpcode(Param1, Param2, Param3)
	db $1C : dw <Param1>,<Param2> : db <Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp1E_UnknownOpcode()
	db $1E
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp20_UnknownOpcode()
	db $20
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp22_UnknownOpcode(Param1, Param2, Param3)
	db $22 : dw <Param1>,<Param2>,<Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp24_UnknownOpcode(Param1, Param2, Param3, Param4)
	db $24 : dw <Param1>,<Param2>,<Param3> : db <Param4>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp26_UnknownOpcode(Param1, Param2, Param3)
	db $26 : dw <Param1>,<Param2> : db <Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp28_UnknownOpcode()
	db $28
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp2A_UnknownOpcode()
	db $2A
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp2C_UnknownOpcode()
	db $2C
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp2E_UnknownOpcode(Param1, Param2)
	db $2E : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp30_UnknownOpcode()
	db $30
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp32_UnknownOpcode(Param1, Param2)
	db $32 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp34_UnknownOpcode(Param1, Param2)
	db $34 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp36_UnknownOpcode()
	db $36
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp38_UnknownOpcode(Param1)
	db $38 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp3A_UnknownOpcode(Param1, Param2, Param3)
	db $3A : dw <Param1>,<Param2> : db <Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp3C_UnknownOpcode(Param1, Param2)
	db $3C : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp3E_UnknownOpcode(Param1)
	db $3E : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp40_UnknownOpcode(Param1, Param2, Param3)
	db $40 : dw <Param1>,<Param2>,<Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp42_UnknownOpcode(Param1, Param2, Param3, Param4)
	db $42 : dw <Param1>,<Param2>,<Param3> : db <Param4>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp44_UnknownOpcode(Param1, Param2, Param3)
	db $44 : dw <Param1>,<Param2> : db <Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp46_UnknownOpcode()
	db $46
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp48_UnknownOpcode()
	db $48
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp4A_UnknownOpcode()
	db $4A
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp4C_UnknownOpcode()
	db $4C
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp4E_UnknownOpcode(Param1)
	db $4E : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp50_UnknownOpcode(Param1, Param2)
	db $50 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp52_UnknownOpcode(Param1, Param2)
	db $52 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp54_UnknownOpcode(Param1, Param2)
	db $54 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp56_UnknownOpcode(Param1, Param2)
	db $56 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp58_UnknownOpcode(Param1, Param2)
	db $58 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp5A_UnknownOpcode(Param1, Param2)
	db $5A : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp5C_UnknownOpcode()
	db $5C
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp5E_UnknownOpcode(Param1, Param2)
	db $5E : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp60_UnknownOpcode()
	db $60
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp62_UnknownOpcode()
	db $62
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp64_UnknownOpcode(Param1, Param2, Param3)
	db $64 : dw <Param1>,<Param2> : db <Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp66_UnknownOpcode()
	db $66
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp68_UnknownOpcode(Param1, Param2)
	db $68 : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp6A_UnknownOpcode(Param1, Param2, Param3, Param4, Param5)
	db $6A : dw <Param1>,<Param2> : db <Param3> : dw <Param4>,<Param5>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp6C_UnknownOpcode(Param1, Param2)
	db $6C : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp6E_UnknownOpcode(Param1, Param2, Param3, Param4)
	db $6E : dw <Param1>,<Param2>,<Param3> : db <Param4>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp70_UnknownOpcode(Param1, Param2, Param3, Param4, Param5)
	db $70 : dw <Param1>,<Param2>,<Param3>,<Param4> : db <Param5>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp72_UnknownOpcode(Param1, Param2, Param3, Param4)
	db $72 : dw <Param1>,<Param2>,<Param3> : db <Param4>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp74_UnknownOpcode(Param1)
	db $74 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp76_UnknownOpcode(Param1)
	db $76 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOp78_UnknownOpcode(Param1, Param2, Param3)
	db $78 : db <Param1>,<Param2> : dw <Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_MIScriptOpFF_UnknownOpcode()
	db $FF
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp00_UnknownOpcode(Param1)
	db $00 : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp01_UnknownOpcode()
	db $01
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp03_UnknownOpcode()
	db $03
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp05_UnknownOpcode(Param1)
	db $05 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp06_UnknownOpcode()
	db $06
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp07_UnknownOpcode(Param1)
	db $07 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp08_UnknownOpcode()
	db $08
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp09_UnknownOpcode(Param1)
	db $09 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp0A_UnknownOpcode()
	db $0A
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp0B_UnknownOpcode(Param1)
	db $0B : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp0D_UnknownOpcode()
	db $0D
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp0E_UnknownOpcode(Param1, Param2)
	db $0E : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp0F_UnknownOpcode()
	db $0F
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp10_UnknownOpcode(Param1, Param2)
	db $10 : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp11_UnknownOpcode(Param1)
	db $11 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp12_UnknownOpcode(Param1)
	db $12 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp13_UnknownOpcode(Param1)
	db $13 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp14_UnknownOpcode()
	db $14
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp15_UnknownOpcode(Param1, Param2)
	db $15 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp16_UnknownOpcode(Param1)
	db $16 : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp19_UnknownOpcode()
	db $19
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp1A_UnknownOpcode()
	db $1A
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp1B_UnknownOpcode()
	db $1B
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp1C_UnknownOpcode()
	db $1C
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp1F_UnknownOpcode(Param1, Param2, Param3)
	db $1F : db <Param1>,<Param2>,<Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp20_UnknownOpcode(Param1)
	db $20 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp21_UnknownOpcode()
	db $21
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp22_UnknownOpcode()
	db $22
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp23_UnknownOpcode()
	db $23
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp24_UnknownOpcode()
	db $24
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp26_UnknownOpcode()
	db $26
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp27_UnknownOpcode()
	db $27
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp2D_UnknownOpcode(Param1)
	db $2D : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp2E_UnknownOpcode()
	db $2E
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp39_UnknownOpcode(Param1)
	db $39 : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp3A_UnknownOpcode(Param1)
	db $3A : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp3B_UnknownOpcode(Param1)
	db $3B : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp3C_UnknownOpcode(Param1)
	db $3C : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp3D_UnknownOpcode()
	db $3D
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp3E_UnknownOpcode(Param1, Param2)
	db $3E : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp44_UnknownOpcode(Param1)
	db $44 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp48_UnknownOpcode(Param1)
	db $48 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp49_UnknownOpcode(Param1, Param2)
	db $49 : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp4A_UnknownOpcode(Param1, Param2)
	db $4A : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp4C_UnknownOpcode()
	db $4C
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp4D_UnknownOpcode(Param1, Param2)
	db $4D : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp50_UnknownOpcode()
	db $50
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp51_UnknownOpcode()
	db $51
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp52_UnknownOpcode()
	db $52
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp53_UnknownOpcode()
	db $53
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp54_UnknownOpcode()
	db $54
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp55_UnknownOpcode()
	db $55
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp56_UnknownOpcode()
	db $56
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp57_UnknownOpcode(Param1, Param2)
	db $57 : db <Param1> : dw <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp58_UnknownOpcode(Param1, Param2)
	db $58 : db <Param1> : dw <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp59_UnknownOpcode(Param1, Param2)
	db $59 : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp5A_UnknownOpcode(Param1, Param2)
	db $5A : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp5B_UnknownOpcode(Param1, Param2)
	db $5B : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp69_UnknownOpcode()
	db $69
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp6A_UnknownOpcode()
	db $6A
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp70_UnknownOpcode()
	db $70
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp71_UnknownOpcode()
	db $71
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp76_UnknownOpcode()
	db $76
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp77_UnknownOpcode(Param1, Param2)
	db $77 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp78_UnknownOpcode(Param1, Param2)
	db $78 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp79_UnknownOpcode(Param1, Param2)
	db $79 : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp7A_UnknownOpcode(Param1, Param2)
	db $7A : db <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp7B_UnknownOpcode()
	db $7B
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp7F_UnknownOpcode(Param1, Param2)
	db $7F : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp80_UnknownOpcode(Param1, Param2)
	db $80 : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp81_UnknownOpcode(Param1, Param2)
	db $81 : db <Param1> : dw <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp82_UnknownOpcode(Param1, Param2)
	db $82 : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp83_UnknownOpcode()
	db $83
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp84_UnknownOpcode()
	db $84
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp85_UnknownOpcode()
	db $85
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp86_UnknownOpcode(Param1, Param2)
	db $86 : db <Param1> : dw <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp87_UnknownOpcode()
	db $87
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp88_UnknownOpcode()
	db $88
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp89_UnknownOpcode()
	db $89
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp8A_UnknownOpcode(Param1, Param2)
	db $8A : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp8B_UnknownOpcode()
	db $8B
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp8C_UnknownOpcode()
	db $8C
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp8D_UnknownOpcode(Param1)
	db $8D : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp8E_UnknownOpcode()
	db $8E
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp8F_UnknownOpcode(Param1)
	db $8F : db <Param1> 
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp90_UnknownOpcode(Param1)
	db $90 : db <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp91_UnknownOpcode()
	db $91
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp92_UnknownOpcode()
	db $92
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp93_UnknownOpcode()
	db $93
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp94_UnknownOpcode()
	db $94
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp95_UnknownOpcode()
	db $95
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp96_UnknownOpcode(Param1)
	db $96 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp97_UnknownOpcode()
	db $97
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp98_UnknownOpcode(Param1)
	db $98 : db <Param1> : dw <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp99_UnknownOpcode(Param1, Param2)
	db $99 : db <Param1> : dw <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp9A_UnknownOpcode(Param1, Param2)
	db $9A : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp9B_UnknownOpcode(Param1, Param2)
	db $9B : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp9C_UnknownOpcode(Param1, Param2)
	db $9C : dw <Param1>,<Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp9D_UnknownOpcode(Param1, Param2)
	db $9D : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp9E_UnknownOpcode(Param1, Param2)
	db $9E : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOp9F_UnknownOpcode()
	db $9F
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA0_UnknownOpcode()
	db $A0
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA1_UnknownOpcode()
	db $A1
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA2_UnknownOpcode(Param1, Param2)
	db $A2 : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA3_UnknownOpcode(Param1, Param2)
	db $A3 : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA4_UnknownOpcode(Param1, Param2, Param3)
	db $A4 : dw <Param1> : db <Param2>,<Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA5_UnknownOpcode(Param1, Param2, Param3)
	db $A5 : dw <Param1> : db <Param2>,<Param3>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA6_UnknownOpcode(Param1, Param2)
	db $A6 : dw <Param1> : db <Param2>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA7_UnknownOpcode(Param1)
	db $A7 : dw <Param1>
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA8_UnknownOpcode()
	db $A8
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpA9_UnknownOpcode()
	db $A9
endmacro

;--------------------------------------------------------------------

macro ISSD_UnkMScriptOpAA_UnknownOpcode(Param1, Param2, Param3)
	db $AA : dw <Param1> : db <Param2>,<Param3>
endmacro

;--------------------------------------------------------------------
