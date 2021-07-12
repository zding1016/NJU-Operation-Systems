/* Real Mode Hello World
.code16
.global start
start:
    movw %cs, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7d00, %ax
    movw %ax, %sp # setting stack pointer to 0x7d00
    pushw $13 # pushing the size to print into stack
    pushw $message # pushing the address of message into stack
    callw displayStr # calling the display function
loop:
    jmp loop

message:
    .string "Hello, World!\n\0"

displayStr:
    pushw %bp
    movw 4(%esp), %ax
    movw %ax, %bp
    movw 6(%esp), %cx
    movw $0x1301, %ax
    movw $0x000c, %bx
    movw $0x0000, %dx
    int $0x10
    popw %bp
    ret
*/
/* Protected Mode Hello World */
#.code16
#
#.global start
#start:
#	movw %cs, %ax
#	movw %ax, %ds
#	movw %ax, %es
#	movw %ax, %ss
#
#.code32
#start32:
#	movw $0x10, %ax # setting data segment selector
#	movw %ax, %ds
#	movw %ax, %es
#	movw %ax, %fs
#	movw %ax, %ss
#loop32:
#	jmp loop32
#
#
#.p2align 2


/* Protected Mode Loading Hello World APP*/
.code16

.global start
start:
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	cli
	inb $0x92, %al
	orb $0x2, %al
	outb %al, $0x92
	data32 addr32 lgdt gdtDesc
	movl %cr0, %eax
	orb $0x1, %al
	movl %eax, %cr0
	data32 ljmp $0x8, $start32


.code32
start32:
	movw $0x10, %ax # setting data segment selector
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %ss
	movw $0x18, %ax # setting graphics data segment selector
	movw %ax, %gs
	
	movl $0x8000, %eax # setting esp
	movl %eax, %esp
	jmp bootMain # jump to bootMain in boot.c

.p2align 2
gdt: 
	.word 0,0
	.byte 0,0,0,0
	.word 0xffff,0
	.byte 0,0x9a,0xcf,0
	.word 0xffff,0
	.byte 0,0x92,0xcf,0
	.word 0xffff,0x8000
	.byte 0x0b,0x92,0xcf,0

gdtDesc: 
	.word (gdtDesc-1-gdt)
	.long gdt
