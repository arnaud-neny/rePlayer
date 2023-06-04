	incdir	asm:
	include	custom.i
	include	rmacros.i
	incdir	include:
	include	misc/deliplayer.i


	section	pretracker,code
	moveq	#-1,d0
	rts
	dc.b	'DELIRIUM'
	dc.l	table
	dc.b	'$VER: PreTracker for UADE 0.1',0
	dc.b	'$COPYRIGHT: Arnaud Neny & Pink/Abyss',0
	dc.b	'$LICENSE: GNU LGPL',0
	even

table	dc.l	DTP_PlayerName,MyPlayerName
	dc.l	DTP_Creator,MyCreator
	dc.l	DTP_SubSongRange,SubSongRange
	dc.l	DTP_InitPlayer,InitPlayer
	dc.l	DTP_InitSound,InitSound
	dc.l	DTP_Interrupt,Interrupt
	dc.l	DTP_EndSound,EndSound
	dc.l	DTP_EndPlayer,EndPlayer
	dc.l	0
MyPlayerName	dc.b	'PreTracker',0
MyCreator	dc.b	'PreTracker for UADE by Arnaud Neny'
	dc.b	' & Pink/Abyss',0
	even



SubSongRange	moveq	#0,d0
	move.l	dtg_ChkData(a5),a0
	add.l	#90,a0
    moveq	#0,d1
	move.b	(a0),d1
    subq.l  #1,d1
	rts

InitPlayer	push	all
	moveq	#0,d0
	move.l	dtg_ChkData(a5),a2

    lea	player(pc),a6
	lea	myPlayer,a0
	lea	mySong,a1
	add.l	(0,a6),a6
	jsr	(a6)		; songInit returns in D0 needed chipmem size

	lea	player(pc),a6
	lea	myPlayer,a0
	lea	chipmem,a1
	lea	mySong,a2
	add.l	(4,a6),a6
	jsr	(a6)		; playerInit

	pull	all
	moveq	#0,d0
	rts

InitSound	push	all
	moveq	#0,d0
	move	dtg_SndNum(a5),d0
	push	d0

	lea	player(pc),a6
	lea	myPlayer,a0
	add.l	(20,a6),a6
	jsr	(a6)		; stop

	lea	player(pc),a6
	lea	myPlayer,a0
	pull	d0
	add.l	(12,a6),a6
	jsr	(a6)		; start song

	lea	player(pc),a6
	lea	myPlayer,a0
	moveq	#64,d0			; volume (0-64)
	add.l	(24,a6),a6
	jsr	(a6)		; setVolume

	pull	all
	moveq	#0,d0
	rts

Interrupt	push	all
	jsr	waitVbl

	lea	player(pc),a6
	lea	myPlayer,a0
	add.l	(8,a6),a6
	jsr	(a6)		; playerTick

	pull	all
	rts

waitVbl	
.0	move.l	$dff004,d0
	and.l	#$1ff00,d0
	cmp.l	#303<<8,d0
	beq	.0

.1	move.l	$dff004,d0
	and.l	#$1ff00,d0
	cmp.l	#303<<8,d0
	bne	.1
	rts

EndSound	push	all
	pull	all
	rts

EndPlayer	move.l	dtg_AudioFree(a5),A0
	jsr	(a0)
	rts

player	incbin	"pretracker.bin"


	section bss,bss
mySong	ds.w	2048/2
myPlayer	ds.l	8*1024/4

	section	chip,bss_c
chipmem	ds.b	256*1024 ; in a real production you should use the returned size of songInit()

	end
