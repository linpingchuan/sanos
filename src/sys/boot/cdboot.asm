;
; cdboot.asm
;
; CD-ROM Boot sector
;
; Copyright (C) 2002 Michael Ringgaard. All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
; 
; 1. Redistributions of source code must retain the above copyright 
;    notice, this list of conditions and the following disclaimer.  
; 2. Redistributions in binary form must reproduce the above copyright
;    notice, this list of conditions and the following disclaimer in the
;    documentation and/or other materials provided with the distribution.  
; 3. Neither the name of the project nor the names of its contributors
;    may be used to endorse or promote products derived from this software
;    without specific prior written permission. 
; 
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
; ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
; OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
; HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
; LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
; OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
; SUCH DAMAGE.
; 

OSLDRSEG    equ 0x9000
OSLDRBASE   equ (OSLDRSEG * 16)

STACKTOP    equ	0xA0000

OSLDRSTART  equ 8
OSLDRSIZE   equ 32

	ORG	0x7c00
	BITS	16
	SECTION	.text

	; Entry point for initial bootstap code
boot:
	jmp 	start
	nop
	
	db	'SANOS   '

ldrsize  dw	OSLDRSIZE
ldrstrt  dd	OSLDRSTART

start:
	; Setup initial environment
	mov	ax, cs
	mov	ds, ax

	; Save boot drive
	mov	[bootdrv], dl

	; Display boot message
	mov	si, bootmsg
	call	print
	
	; Copy os loader from ram boot image 
	push	ds

	mov	cx, [ldrsize]	    ; cx = number of bytes to copy
	shl	cx, 9	            ; convert from sectors to bytes

	mov	eax, [ldrstrt]	    ; ds = loader start segment
	shl	eax, 5		    ; convert from sectors to segments
	add	eax, 0x7c00
	mov	ds, ax

	mov	ax, OSLDRSEG	    ; es = loader segment
	mov	es, ax

	xor	di, di
	xor	si, si
	cld
	rep movsb
	pop	ds

;xxx:
;	mov	al, '.'
;	call	printchar
;	jmp	xxx

	; Move system into 32-bit protected mode
	cli			    ; no interrupts allowed

	; enable A20
	call	empty8042
	mov	al, 0xd1	    ; command write
	out	0x64, al
	call	empty8042
	mov	al,0xdf	  	    ; A20 on
	out	0x60, al
	call	empty8042

	; Load idt, gdt and ldt
	lidt	[idtsel]	    ; load idt with 0,0
	lgdt	[gdtsel]	    ; load gdt with whatever appropriate

	; Switch to protected mode
	mov	ax, 0x0001
	lmsw	ax

	; Initialize segment registers
	mov	ax, flat_data - gdt
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax

	; Set code segment and clear prefetch
	jmp	dword (flat_code - gdt):start32

start32:
	BITS	32
	; Setup stack
	mov	esp, STACKTOP

	; Clear flags
	push	dword 2
	popfd

	; Calculate entrypoint
	mov	eax, [OSLDRBASE + 0x3c]
	mov	eax, [eax + OSLDRBASE + 0x28]
	add	eax, OSLDRBASE

	; Set boot drive to 0xFF to signal boot from CD
	mov	ebx, 0xFF

	; Push os loader load address, boot drive and initial ram disk image
	push	dword 0x7C00
	push	ebx
	push	dword OSLDRBASE

	; Call startup code in os loader
	call	eax

	; We never return here
	cli
	hlt

	BITS	16
		
	; Empty keyboard command queue

empty8042:
	call	delay
	in	al,0x64		; 8042 status port
	test	al,2		; is input buffer full?
	jnz	empty8042	; yes - loop
	ret

	; delay is needed after doing i/o
delay:
	dw	0x00eb, 0x00eb
	ret


	; Print string to console
	; si = ptr to first character of a null terminated string
print:
	push	ax
	cld
nextchar:
	mov	al, [si]
	cmp	al, 0
	je	printdone
	call	printchar
	inc	si
	jmp 	nextchar
printdone:
	pop	ax
	ret

	; Print a single character to the console
	; al = character to be printed
printchar:
	mov	ah, 0x0e
	int	0x10
	ret

	; Global descriptor table

D_DATA		equ	0x1000
D_CODE		equ	0x1800

D_WRITE		equ	0x200
D_READ		equ	0x200

D_BIG		equ	0x40
D_BIG_LIM	equ	0x80

D_PRESENT	equ	0x8000

%macro segdesc 3
	dw	%2
	dw	%1
	db	(%1) >> 16
	db	((%3) | D_PRESENT) >> 8
	db	((%3) & 0xff) | ((%2) >> 16)
	db	(%1) >> 24
%endmacro

idtsel:
	dw	0		; idt limit = 0
	dw	0,0		; idt base = 0L

gdtsel:

	dw	gdtlen
	dd	gdt

	;align	8
gdt:

null_desc	segdesc	0,0,0
flat_code	segdesc	0, 0xFFFFF, D_CODE | D_READ | D_BIG | D_BIG_LIM
flat_data	segdesc	0, 0xFFFFF, D_DATA | D_WRITE | D_BIG | D_BIG_LIM

gdtlen	  	equ $ - gdt - 1

	; Variables

bootdrv	db	0

	; Message strings
bootmsg:
	db	10, 13, 'Booting system from CD', 0

	; Boot signature
	times 	510-($-$$) db 0
	dw 	0xAA55