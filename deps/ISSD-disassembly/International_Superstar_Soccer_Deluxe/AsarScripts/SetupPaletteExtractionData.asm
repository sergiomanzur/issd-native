; This will dump the data for an asar patch that will be applied to the USA ISSD ROM. Said patch will set up the asset extraction data of all the palettes in the specified range when applied to the ROM.
; The reason I'm generating a patch and not the tables directly is because of asar limitations. I don't think it's possible for asar to resolve commands through a define while in a print statement.
; Also, it may take a second before asar starts displaying anything on the command line. In addition, you'll need to replace the ' with " in the output patch, and the % with " in the second output patch, otherwise asar won't assemble the patch.
; Lastly, the last data block will likely be larger than it should be and must be cleaned up.

!PtrStartOffset = $898000
!PtrEndOffset =	$89FA42

lorom
!LoopCounter = 0
print "lorom"

!CurrentOffset #= !PtrStartOffset
while !CurrentOffset+!LoopCounter < !PtrEndOffset
	!PALSize1 #= read2(!CurrentOffset+!LoopCounter+$00)
	print "print '	dl $000000,PaletteFile',hex(!CurrentOffset+!LoopCounter, 6),'_Ptrs,PaletteFile',hex(!CurrentOffset+!LoopCounter, 6),',PaletteFile',hex(!CurrentOffset+!LoopCounter, 6),'End'"
		!LoopCounter #= !LoopCounter+!PALSize1+3
	if !CurrentOffset+!LoopCounter == $89F168
		!CurrentOffset #= $89F45E
		!LoopCounter #= 0
	endif
endif
!LoopCounter #= 0
!CurrentOffset #= !PtrStartOffset
print "print 'PalettePointersEnd:'"
print "print ''"
while !CurrentOffset+!LoopCounter < !PtrEndOffset
	!PALSize1 #= read2(!CurrentOffset+!LoopCounter+$00)
	print "print 'PaletteFile',hex(!CurrentOffset+!LoopCounter, 6),'_Ptrs:'"
	print "print '	dw $03 : dw $0000'"
	print "print '	dl $B1C48E,$B1C490		;\ TPL Header'"
	print "print '	dl $81E1CD,$81E1CF		;/'"
	print "print '	dl $',hex(!CurrentOffset+!LoopCounter+2, 6),',$',hex(!CurrentOffset+!LoopCounter+!PALSize1+3, 6),'		; '"
	print "print ''"
		!LoopCounter #= !LoopCounter+!PALSize1+3
	if !CurrentOffset+!LoopCounter == $89F168
		!CurrentOffset #= $89F45E
		!LoopCounter #= 0
	endif
endif
!LoopCounter #= 0
!CurrentOffset #= !PtrStartOffset
print "print ''"
while !CurrentOffset+!LoopCounter < !PtrEndOffset
	!PALSize1 #= read2(!CurrentOffset+!LoopCounter+$00)
	print "print 'PaletteFile',hex(!CurrentOffset+!LoopCounter, 6),':'"
	print "print '	db %PaletteFile',hex(!CurrentOffset+!LoopCounter, 6),'.tpl%'"
	print "print 'PaletteFile',hex(!CurrentOffset+!LoopCounter, 6),'End:'"
		!LoopCounter #= !LoopCounter+!PALSize1+3
	if !CurrentOffset+!LoopCounter == $89F168
		!CurrentOffset #= $89F45E
		!LoopCounter #= 0
	endif
endif
