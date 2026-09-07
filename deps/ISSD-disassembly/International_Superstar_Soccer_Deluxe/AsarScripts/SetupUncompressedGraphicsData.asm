; This will dump the data for an asar patch that will be applied to the USA ISSD ROM. Said patch will set up the header data of all the poses in the specified range (and also generate an asset extraction line and incbin command) when applied to the ROM.
; The reason I'm generating a patch and not the tables directly is because of asar limitations. I don't think it's possible for asar to resolve commands through a define while in a print statement.
; Also, it may take a second before asar starts displaying anything on the command line. In addition, you'll need to replace the ' with " in the output patch, and the % with " in the second output patch, otherwise asar won't assemble the patch.
; Lastly, the last data block will likely be larger than it should be and must be cleaned up.

!PtrStartOffset = $8FFACF
!PtrEndOffset =	$8FFFFF

lorom

!LoopCounter = 0
print "lorom"

while !PtrStartOffset+!LoopCounter < !PtrEndOffset
	!GFXSize1 #= read2(!PtrStartOffset+!LoopCounter+$00)
	!GFXSize2 #= read2(!PtrStartOffset+!LoopCounter+!GFXSize1+2)
	!PrintData = ""
	print "print 'DATA_',hex(!PtrStartOffset+!LoopCounter),':'"
	if !GFXSize1 == $0000
		!LoopCounter #= !PtrEndOffset
	elseif !GFXSize1 == $FFFF
		!LoopCounter #= !PtrEndOffset
	else
		print "print '	dw .TopHalfEnd-DATA_',hex(!PtrStartOffset+!LoopCounter),'-$02'"
		print "print '	;incbin %Graphics/GFX_Sprite_XXXX.bin%:0-',hex(!GFXSize1)"
		print "print '.TopHalfEnd:'"
		print "print '	dw .BottomHalfEnd-.TopHalfEnd-$02'"
		print "print '	;incbin %Graphics/GFX_Sprite_XXXX.bin%:',hex(!GFXSize1),'-'"
		print "print '.BottomHalfEnd:'"
		print "print '	;dl $',hex(!PtrStartOffset+!LoopCounter+2),',$',hex(!PtrStartOffset+!LoopCounter+!GFXSize1+2),',GFX_Sprite_XXXX,GFX_Sprite_XXXXEnd'"
		print "print '	;dl $',hex(!PtrStartOffset+!LoopCounter+!GFXSize1+4),',$',hex(!PtrStartOffset+!LoopCounter+!GFXSize1+!GFXSize2+4),',GFX_Sprite_XXXX,GFX_Sprite_XXXXEnd'"
		print "print ''"
		!LoopCounter #= !LoopCounter+!GFXSize1+!GFXSize2+4
	endif
endif
