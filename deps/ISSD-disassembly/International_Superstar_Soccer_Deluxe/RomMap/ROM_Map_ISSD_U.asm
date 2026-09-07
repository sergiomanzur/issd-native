
macro ISSD_GameSpecificAssemblySettings()
	!ROM_ISSD_USA = $0001							;\ These defines assign each ROM version with a different bit so version difference checks will work. Do not touch them!
	!ROM_ISSD_PAL = $0002							;|
	!ROM_ISSD_Japan = $0004						;/

	%SetROMToAssembleForHack(ISSD_U, !ROMID)
endmacro

macro ISSD_LoadGameSpecificMainSNESFiles()
	incsrc ../SPC700/ARAMPtrs_ISSD.asm
	incsrc ../Misc_Defines_ISSD.asm
	incsrc ../RAM_Map_ISSD.asm
	incsrc ../Routine_Macros_ISSD.asm
	incsrc ../SNES_Macros_ISSD.asm
	%LoadExtraRAMFile("SRAM_Map_ISSD.asm", !GameID, ISSD)
endmacro

macro ISSD_LoadGameSpecificMainSPC700Files()
	incsrc ../SPC700/ARAM_Map_ISSD.asm
	incsrc ../Misc_Defines_ISSD.asm
	incsrc ../SPC700/SPC700_Routine_Macros_ISSD.asm
	incsrc ../SPC700/SPC700_Macros_ISSD.asm
endmacro

macro ISSD_LoadGameSpecificMainExtraHardwareFiles()
endmacro

macro ISSD_LoadGameSpecificMSU1Files()
endmacro

macro ISSD_GlobalAssemblySettings()
	!Define_Global_ApplyAsarPatches = !FALSE
	!Define_Global_ApplyDefaultPatches = !TRUE
	!Define_Global_InsertRATSTags = !TRUE
	!Define_Global_IgnoreCodeAlignments = !FALSE
	!Define_Global_IgnoreOriginalFreespace = !FALSE
	!Define_Global_CompatibleControllers = !Controller_StandardJoypad|!Controller_Multiplayer5
	!Define_Global_DisableROMMirroring = !FALSE
	!Define_Global_CartridgeHeaderVersion = $02
	!Define_Global_FixIncorrectChecksumHack = !FALSE
	!Define_Global_ROMFrameworkVer = 1
	!Define_Global_ROMFrameworkSubVer = 3
	!Define_Global_ROMFrameworkSubSubVer = 1
	!Define_Global_AsarChecksum = $0000
	!Define_Global_LicenseeName = "Konami"
	!Define_Global_DeveloperName = "Konami"
	!Define_Global_ReleaseDate = "November 1995"
	!Define_Global_BaseROMMD5Hash = "345ddedcd63412b9373dabb67c11fc05"

	!Define_Global_MakerCode = "A4"
	!Define_Global_GameCode = "AWJE"
	!Define_Global_ReservedSpace = $00,$00,$00,$00,$00,$00
	!Define_Global_ExpansionFlashSize = !ExpansionMemorySize_0KB
	!Define_Global_ExpansionRAMSize = !ExpansionMemorySize_0KB
	!Define_Global_IsSpecialVersion = $00
	!Define_Global_InternalName = "SUPERSTAR SOCCER 2"
	!Define_Global_ROMLayout = !ROMLayout_LoROM_FastROM
	!Define_Global_ROMType = !ROMType_ROM
	!Define_Global_CustomChip = !Chip_None
	!Define_Global_ROMSize1 = !ROMSize_2MB
	!Define_Global_ROMSize2 = !ROMSize_0KB
	!Define_Global_SRAMSize = !SRAMSize_0KB
	!Define_Global_Region = !Region_NorthAmerica
	!Define_Global_LicenseeID = $33
	!Define_Global_VersionNumber = $00
	!Define_Global_ChecksumCompliment = !Define_Global_Checksum^$FFFF
	!Define_Global_Checksum = $9A13
	!UnusedNativeModeVector1 = $FFFF
	!UnusedNativeModeVector2 = $FFFF
	!NativeModeCOPVector = CODE_80FF97
	!NativeModeBRKVector = CODE_80FF97
	!NativeModeAbortVector = CODE_80FF97
	!NativeModeNMIVector = CODE_80FF9B
	!NativeModeResetVector = CODE_80FF90
	!NativeModeIRQVector = CODE_80FF97
	!UnusedEmulationModeVector1 = $FFFF
	!UnusedEmulationModeVector2 = $FFFF
	!EmulationModeCOPVector = CODE_80FF97
	!EmulationModeBRKVector = CODE_80FF97
	!EmulationModeAbortVector = CODE_80FF97
	!EmulationModeNMIVector = CODE_80FF9B
	!EmulationModeResetVector = CODE_80FF90
	!EmulationModeIRQVector = CODE_80FF97
endmacro

macro ISSD_LoadROMMap()
	%ISSDBank80Macros(!BANK_00, !BANK_00)
	%ISSDBank81Macros(!BANK_01, !BANK_01)
	%ISSDBank82Macros(!BANK_02, !BANK_02)
	%ISSDBank83Macros(!BANK_03, !BANK_03)
	%ISSDBank84Macros(!BANK_04, !BANK_04)
	%ISSDBank85Macros(!BANK_05, !BANK_05)
	%ISSDBank86Macros(!BANK_06, !BANK_06)
	%ISSDBank87Macros(!BANK_07, !BANK_07)
	%ISSDBank88Macros(!BANK_08, !BANK_08)
	%ISSDBank89Macros(!BANK_09, !BANK_09)
	%ISSDBank8AMacros(!BANK_0A, !BANK_0A)
	%ISSDBank8BMacros(!BANK_0B, !BANK_0B)
	%ISSDBank8CMacros(!BANK_0C, !BANK_0C)
	%ISSDBank8DMacros(!BANK_0D, !BANK_0D)
	%ISSDBank8EMacros(!BANK_0E, !BANK_0E)
	%ISSDBank8FMacros(!BANK_0F, !BANK_0F)
	%ISSDBank90Macros(!BANK_10, !BANK_10)
	%ISSDBank91Macros(!BANK_11, !BANK_11)
	%ISSDBank92Macros(!BANK_12, !BANK_12)
	%ISSDBank93Macros(!BANK_13, !BANK_13)
	%ISSDBank94Macros(!BANK_14, !BANK_14)
	%ISSDBank95Macros(!BANK_15, !BANK_15)
	%ISSDBank96Macros(!BANK_16, !BANK_16)
	%ISSDBank97Macros(!BANK_17, !BANK_17)
	%ISSDBank98Macros(!BANK_18, !BANK_18)
	%ISSDBank99Macros(!BANK_19, !BANK_19)
	%ISSDBank9AMacros(!BANK_1A, !BANK_1A)
	%ISSDBank9BMacros(!BANK_1B, !BANK_1B)
	%ISSDBank9CMacros(!BANK_1C, !BANK_1C)
	%ISSDBank9DMacros(!BANK_1D, !BANK_1D)
	%ISSDBank9EMacros(!BANK_1E, !BANK_1E)
	%ISSDBank9FMacros(!BANK_1F, !BANK_1F)
	%ISSDBankA0Macros(!BANK_20, !BANK_20)
	%ISSDBankA1Macros(!BANK_21, !BANK_21)
	%ISSDBankA2Macros(!BANK_22, !BANK_22)
	%ISSDBankA3Macros(!BANK_23, !BANK_23)
	%ISSDBankA4Macros(!BANK_24, !BANK_24)
	%ISSDBankA5Macros(!BANK_25, !BANK_25)
	%ISSDBankA6Macros(!BANK_26, !BANK_26)
	%ISSDBankA7Macros(!BANK_27, !BANK_27)
	%ISSDBankA8Macros(!BANK_28, !BANK_28)
	%ISSDBankA9Macros(!BANK_29, !BANK_29)
	%ISSDBankAAMacros(!BANK_2A, !BANK_2A)
	%ISSDBankABMacros(!BANK_2B, !BANK_2B)
	%ISSDBankACMacros(!BANK_2C, !BANK_2C)
	%ISSDBankADMacros(!BANK_2D, !BANK_2D)
	%ISSDBankAEMacros(!BANK_2E, !BANK_2E)
	%ISSDBankAFMacros(!BANK_2F, !BANK_2F)
	%ISSDBankB0Macros(!BANK_30, !BANK_30)
	%ISSDBankB1Macros(!BANK_31, !BANK_31)
	%ISSDBankB2Macros(!BANK_32, !BANK_32)
	%ISSDBankB3Macros(!BANK_33, !BANK_33)
	%ISSDBankB4Macros(!BANK_34, !BANK_34)
	%ISSDBankB5Macros(!BANK_35, !BANK_35)
	%ISSDBankB6Macros(!BANK_36, !BANK_36)
	%ISSDBankB7Macros(!BANK_37, !BANK_37)
	%ISSDBankB8Macros(!BANK_38, !BANK_38)
	%ISSDBankB9Macros(!BANK_39, !BANK_39)
	%ISSDBankBAMacros(!BANK_3A, !BANK_3A)
	%ISSDBankBBMacros(!BANK_3B, !BANK_3B)
	%ISSDBankBCMacros(!BANK_3C, !BANK_3C)
	%ISSDBankBDMacros(!BANK_3D, !BANK_3D)
	%ISSDBankBEMacros(!BANK_3E, !BANK_3E)
	%ISSDBankBFMacros(!BANK_3F, !BANK_3F)
endmacro

;#############################################################################################################
;#############################################################################################################

macro ISSD_LoadSPC700ROMMap()
	%SPC700_ISSD_SPC700_Engine($0200)
endmacro

;#############################################################################################################
;#############################################################################################################

macro ISSD_LoadSuperFXROMMap()
endmacro

;#############################################################################################################
;#############################################################################################################

macro ISSD_LoadMSU1ROMMap()
endmacro

;#############################################################################################################
;#############################################################################################################
