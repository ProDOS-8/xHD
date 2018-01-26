*=================================================
* xHD serial driver
* By John Brooks 10/28/2015
* Virtual disk drive based on ideas from Terence J. Boldt
*=================================================
			lst		off

			typ		bin
			dsk		xHD

ZpPageNum	=		$3a
ZpChecksum	=		$3b
ZpReadEnd	=		$3c
ZpTemp		=		$3e

ZpDrvrCmd	=		$42
ZpDrvrUnit	=		$43
ZpDrvrBufPtr =		$44
ZpDrvrBlk	=		$46

P8ErrIoErr	=		$27

TxtLight	=		$e00427


P8CmdQuit	=		$65

P8Mli		=		$BF00
P8DevCnt	=		$BF31
P8DevLst	=		$BF32

IoSccCmdB	=		$C038
IoSccCmdA	=		$C039
IoSccDataB	=		$C03A
IoSccDataA	=		$C03B

IoRomIn		=		$C081
IoLcBank2	=		$C08B

RomStrOut	=		$DB3A		;YA=C string

*-------------------------------------------------

			org		$2000
Start

:MoveE0Driver
			lda		IoLcBank2
			lda		IoLcBank2

			; Copy E0Driver
			clc
			xce
			rep		#$30
			ldx		#E0DriverBin1-ToE0Driver+E0DriverBin2
			ldy		#E0Driver
			lda		#E0DriverEnd-E0Driver-1
			mvn		E0DriverBin1, E0Driver
			ldx		#E0DriverBin2
			ldy		#ToE0Driver
			lda		#E0DriverBin1-ToE0Driver-1
			mvn		E0DriverBin2, ToE0Driver
			;phk
			;plb

			lda		$BF10			;Get 'null' E0Driver address
			ldy		#2
:FindSlot	cmp		$BF10,y
			bne		:NotEmpty
			cmp		$BF20,y
			beq		:GotSlot
:NotEmpty	iny
			iny
			cpy		#$10
			bcc		:FindSlot

			ldy		#2				;All slots filled. Overwrite slot 1 devices
:GotSlot			
			lda		#ToE0Driver
			sta		$BF10,y
			sta		$BF20,y

			phy
			
			sep		#$30
			tya
			ora		#$10			;Set Drive 2 (hi) bit
			xba						;Save in high byte
			tya						;Set Slot,Drive 1 in low byte
			rep		#$20
			asl						;Shift *8 to finish slot*16
			asl
			asl
			
			ldy		P8DevCnt
			sta		P8DevLst+1,Y
			iny
			iny
			sty		P8DevCnt

			sec
			xce
			mx		%11
			
			lda		IoRomIn	; Disable LC

			lda		#StrSlot
			ldy		#>StrSlot
			jsr		RomStrOut
			pla
			lsr
			jsr		$FDE5
Exit
			tsc
			cmp		#$fc
			bcc		:GotBasic
:Quit
			jsr		P8Mli	; $BF00
			db		P8CmdQuit
			dw		:QuitParms
:QuitParms
			db		4
			ds		7
:GotBasic
			rts

StrSlot
			asc		'Modem xHD in Slot ',00
*-------------------------------------------------
E0DriverBin2
			org		$00d7a0
ToE0Driver
			ldx		#0
			clc
			xce
			jsl		>E0Driver
E0DriverReturn			
			xce
			sec
			xce
			rts
CmdHdr		;asc		"E"
CurCmd		db		0
HdrBlk		dw		0
HdrChecksum	db		0
HdrCopy		ds		4-1
;TempDate	ds		4
HdrCopyCksum dw		1
			
			
*-------------------------------------------------
E0DriverBin1
			org		$e0bd00

E0Driver
			jmp		(GSCmd,x)
GSCmd
			dw		xHdClient

xHdClient
			lda		#2
			bit		ZpDrvrUnit
			bpl		:GotZpDrvrUnit
			asl
:GotZpDrvrUnit
			sta		CurCmd			;2=Drive1, 4=Drive2

			lda		#15				;Are 8530 interrupts disabled?
			sta		IoSccCmdB
			lda		IoSccCmdB
			beq		:ConfigOK		;If not then SCC is set up by firmware, so reset it

			bit		$c030
			ldx		#-SccInitLen
:InitSCC	
			ldal	SccInitTblEnd-$100,x
			sta		IoSccCmdB
			inx
			bne		:InitSCC

:ConfigOK

			do		0
			phx
			ldal	$227fff
			tax
			lda		ZpDrvrCmd
			stal	$228000,x
			lda		ZpDrvrBlk
			stal	$228100,x
			lda		ZpDrvrBlk+1
			stal	$228200,x
			inx
			txa
			stal	$227fff
			plx
			fin
			
			lda		ZpDrvrCmd
			beq		:DoStatus		;0=status
			dec
			beq		ReadBlock		;1=read block
			dec
			bne		:NoCmd			;2=write block
			jmp		WriteBlock
:DoStatus
			ldx		#$ff			;TODO - returns 32MB HD regardless of image size
			txy
:NoCmd
			lda		#0				; no error
			clc
			rtl

;230k baud
SccInitTbl
			db		4,	%01000100	; 4: x16 clock, 1 stop, no parity
			db		3,	%11000000	; 3: 8 data bits, auto enables off, Rx off
			db		5,	%01100010	; 5: DTR on, 8 data bits, no break, Tx off, RTS off
			db		11,	%00000000	;11: external clock
			db		14,	%00000000	;14: no loopback
			db		3,	%11000001	; 3: 8 data bits, Rx on
			db		5,	%01101010	; 5: DTR on; Tx on
			db		15,	%00000000	;15: no interrupts
SccInitTblEnd
SccInitLen	=		SccInitTblEnd-SccInitTbl

*-------------------------------------------------
			mx		%11
ReadBlock
			rep		#$10
			xba						;ah=0
			lda		CurCmd
			lsr
			ora		#$30
			stal	TxtLight
			inc		CurCmd			;3=drive1, 5=drive2
			ldx		ZpDrvrBlk
			stx		HdrBlk
			jsr		ClearRx
			jsr		SendCmd
			stz		ZpChecksum

		do	1
			ldy		#HdrCopy
			ldx		#HdrCopy+5-1
;			ldx		#HdrCopy+9
			stx		ZpReadEnd
			jsr		ReadBytes
			bcc		ReadError
			lda		ZpChecksum
			bne		ReadError
			
			ldy		#2
:ErrChk		lda		CmdHdr,y
			cmp		HdrCopy,y
			bne		ReadError
			dey
			bpl		:ErrChk
			
		fin

		do	0
			jsr		ReadOneByte
			cmp		#"E"
			bne		ReadError
			jsr		ReadOneByte
			cmp		CurCmd
			bne		ReadError
			jsr		ReadOneByte
			cmp		ZpDrvrBlk
			bne		ReadError
			jsr		ReadOneByte
			cmp		ZpDrvrBlk+1
			bne		ReadError
			jsr		ReadOneByte
			sta		TempDate
			jsr		ReadOneByte
			sta		TempDate+1
			jsr		ReadOneByte
			sta		TempDate+2
			jsr		ReadOneByte
			sta		TempDate+3
			jsr		ReadOneByte
			lda		ZpChecksum
			bne		ReadError
		
			stz		ZpChecksum
		fin

			rep		#$21
			ldy		ZpDrvrBufPtr
			tya
			adc		#$200
			sta		ZpReadEnd
			lda		#0
			sep		#$20
			jsr		ReadBytes
			bcc		ReadError

			jsr		ReadOneByte		;Read checksum
			bcc		ReadError		;Err if P8Timeout
			lda		ZpChecksum		;Chksum==0?
			bne		ReadError		;Err if bad chksum

			tsb		TxtLight
			sep		#$10
			clc
			rtl

*-------------------------------------------------
			mx		%10
ReadError
			jsr		ClearRx
			lda		#0
			tsb		TxtLight
			sep		#$10
			lda		#P8ErrIoErr
			sec
			rtl


*-------------------------------------------------
			mx		%11
WriteBlock
			rep		#$10
			xba						;ah=0
			lda		#$17
			stal	TxtLight
			ldx		ZpDrvrBlk
			stx		HdrBlk
			jsr		ClearRx
			jsr		SendCmd
			stz		ZpChecksum

			ldy		ZpDrvrBufPtr
			sty		ZpReadEnd
			inc		ZpReadEnd+1
			inc		ZpReadEnd+1		;Send 2x pages = 512 bytes
			jsr		WriteBytes

			sta		ZpPageNum		;Save block checksum

			ldy		#ZpChecksum
			sty		ZpReadEnd
			jsr		WriteBytes

		do	1
			ldy		#HdrCopy
			ldx		#HdrCopy+5-1
;			ldx		#HdrCopy+9
			stx		ZpReadEnd
			jsr		ReadBytes
			bcc		ReadError

			cmp		ZpPageNum		;block ZpChecksum
			bne		ReadError
			
			ldy		#2
:ErrChk		lda		CmdHdr,y
			cmp		HdrCopy,y
			bne		ReadError
			dey
			bpl		:ErrChk
			
		fin

		do	0
;			jsr		ReadOneByte
;			bcc		WriteError
;			cmp		#"E"
;			bne		WriteError
			jsr		ReadOneByte
			bcc		WriteError
			cmp		CurCmd			;Write
			bne		WriteError
			jsr		ReadOneByte		;Block low
			bcc		WriteError
			cmp		ZpDrvrBlk
			bne		WriteError
			jsr		ReadOneByte		;Block high
			bcc		WriteError
			cmp		ZpDrvrBlk+1
			bne		WriteError
			lda		ZpPageNum		;Restore block ZpChecksum
			sta		ZpChecksum
			jsr		ReadOneByte		;Block data checksum
			bcc		WriteError
			lda		ZpChecksum
			bne		WriteError
		fin
		
			tsb		TxtLight

			sep		#$10
			clc
			rtl

*-------------------------------------------------
			mx		%10
WriteError
			jsr		ClearRx
			lda		#0
			tsb		TxtLight
			sep		#$10
			lda		#P8ErrIoErr
			sec
			rtl

*-------------------------------------------------
			mx		%10
SendCmd
			inc		$c034
			stz		ZpChecksum
		
			ldy		#CmdHdr
			ldx		#HdrChecksum
			stx		ZpReadEnd
			jsr		WriteBytes
			sta		HdrChecksum
			dec		$c034
			;Fall through to send checksum byte
			
*-------------------------------------------------
			mx		%10
WriteBytes
			tsx						;Init timeout
			clc
:Loop
			inx						;P8Timeout++
			bmi		:Exit
			lda		IoSccCmdB		;Reg 0
			and		#%00100100		;Chk bit 5 (ready to send) & bit 2 (HW handshake)
			eor		#%00100100
			bne		:Loop

			lda		0,y				;Get byte
			sta		IoSccDataB		;Tx byte

			eor		ZpChecksum
			sta		ZpChecksum		;Update cksum
			
			iny
			cpy		ZpReadEnd
			bcc		WriteBytes

			rts
			
*-------------------------------------------------
			mx		%10
ReadOneByte
			ldy		#$C07f
			sty		ZpReadEnd
			;fall through to ReadBytes

*-------------------------------------------------
			mx		%10
ReadBytes
:ReadByte
			tsx						;Init timeout
:Loop
			inx						;P8Timeout++
			bmi		:Exit
			lda		IoSccCmdB		;Chk reg 0 bit 0
			lsr
			bcc		:Loop

			lda		IoSccDataB		;Byte received
			sta		0,y				;Store it
			tax						;Save in case this is a 1 byte read
			
			eor		ZpChecksum
			sta		ZpChecksum		;Update cksum
			
			iny
			cpy		ZpReadEnd
			bcc		:ReadByte
			
			txa						;Return last byte read
:Exit		rts

*-------------------------------------------------
			mx		%10
ClearRx

:ClearFifo	
			lda		#1
			bit		IoSccCmdB		;Chk reg 0 bit 0
			beq		:Done

			sta		IoSccCmdB		;Read reg 1
			lda		#$30			;Chk & Clear overrun
			bit		IoSccCmdB		;Chk bit 5 for RX OVERRUN
			beq		:NotOverrun
			sta		IoSccCmdB
			stz		IoSccCmdB
:NotOverrun
			lda		IoSccDataB		;Byte received
			bra		:ClearFifo
:Done
			rts

E0DriverEnd

*-------------------------------------------------
			do		0
			org		$2ec
			dsk		SccBoot1

sta_di		mac		; sta (]1)
			db		$92,]1&$ff
			eom
			
			;org		$60
			mx		%11
BootInitScc
			;pea		#$0300
			;pld
			jsr		BootInitScc1
			dec		InitMOD+1 ;&$ff
			stz		SccDataMod ;&$ff
BootInitScc1
			ldx		#BootstrapDataEnd-BootstrapData-1
InitSccLoop
			lda		BootstrapData,x ;&$ff,x
InitMOD
			sta		IoSccCmdA
			dex
			bpl		InitSccLoop

BootStrap
			clc
			xce
			rep		#$30
			pea		#$0300
			pld
:Respond			
:WaitSend
			lda		(pIoSccCmdB&$ff)
			ora		#$2020!$ffff
			inc
			bne		:WaitSend

			;Send 'GS' packet header
			lda		PacketHdr&$ff
			sta_di	pIoSccDataB		;sta	(pIoSccDataB&$ff)

			;Get 'GS' response from server
			jsr		BootGetWord
			cmp		PacketHdr&$ff
			bne		:WaitSend			;If server didn't respond, then echo packet header, then retry
			
:ReadCmd
			;Read xfer word length (0=exit)
			jsr		BootGetWord
			beq		:Exit
			tax							;word length

			;Read bank & page of xfer dest
			jsr		BootGetWord
			sta		pDest+1&$ff

			;Read byte offset of xfer dest
			jsr		BootGetWord
			tay

:ReadWord
			jsr		BootGetWord
:DestMOD
			sta		[pDest&$ff],y
			iny
			iny
			dex
			bne		:ReadWord
			bra		:Respond

:Exit
			;Push last xfer bank/page on stack and jump to Adr+1
			pei		pDest+1&$ff
			phk
			rtl


BootGetWord
			phx
			ldx		#1000			;P8Timeout duration
:Loop
			dex
			bmi		:P8Timeout
			lda		(pIoSccCmdB&$ff)
			ora		#$2121!$ffff
			inc
			bne		:Loop
:P8Timeout
			plx
			lda		(pIoSccDataB&$ff)
			sta_di	pIoSccDataB		;sta	(pIoSccDataB&$ff)
			
			rts

PacketHdr
			dw		'GS'
pIoSccCmdB
			dw		IoSccCmdB
pIoSccDataB
			dw		IoSccDataB


BootstrapData
			; Read last-to-first
			;db		%00000000, 15	;15: no interrupts
			db		%01101010, 5	; 5: DTR enabled; Tx on
			db		%11000001, 3	; 3: 8 data bits, Rx on
			;db		%00000000, 14	;14: no loopback
pDest
SccDataMod
			db		%10000000, 11	;11: external clock
			db		%01100010, 5	; 5: DTR on, 8 data bits, no break, Tx off, RTS off
			db		%11000000, 3	; 3: 8 data bits, auto enables off, Rx off
			db		%01000100, 4	; 4: x16 clock, 1 stop, no parity
BootstrapDataEnd
			fin
*-------------------------------------------------

			do		0
			org		$300
Begin
			pea		#*&$ff00
			pld
			lda		(:ptr&$ff)
			lda		(:ptr&$ff),y
			lda		[:ptr&$ff],y

			; lda works, why do these fail?
			;adc		(:ptr&$ff)
			;and		(:ptr&$ff)
			;cmp		(:ptr&$ff)
			;eor		(:ptr&$ff)
			;ora		(:ptr&$ff)
			;sbc		(:ptr&$ff)
			;sta		(:ptr&$ff)
:ptr
			dw		$2000

KeyClr equ $e0c010
			lda		>KeyClr
			lda		|KeyClr
			lda		<KeyClr
			lda		KeyClr&$ff
			sta		>KeyClr
			sta		|KeyClr
			sta		<KeyClr
			sta		KeyClr&$ff
			fin

*-------------------------------------------------
