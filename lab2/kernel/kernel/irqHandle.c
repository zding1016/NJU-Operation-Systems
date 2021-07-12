#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;


void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
		case 0xd:
			GProtectFaultHandle(tf);
			break;
		case 0x21:
			KeyboardHandle(tf);
			break;
		case 0x80:
			syscallHandle(tf);
			break;
		case -1:
			break;
		default:assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();
	uint16_t data=0;
	int pos=0;
	char character;
	if(code == 0xe)
	{ // 退格符
		// TODO: 要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if(bufferTail>=bufferHead&&displayCol>0)
		{
			displayCol--;
			pos=(displayRow*80+displayCol)*2;
			asm volatile("movw %0,(%1)"::"r"(data),"r"(pos+0xb8000));
			keyBuffer[bufferTail]=0;
			bufferTail--;
		}
	}
	else if(code == 0x1c)
	{ // 回车符
		// TODO: 处理回车情况
		displayRow++;
		displayCol=0;
		if(displayRow==25)
		{
			displayRow--;
			scrollScreen();
		}
		bufferTail++;
		keyBuffer[bufferTail]=13;
		bufferTail=bufferTail%MAX_KEYBUFFER_SIZE;
	}
	else if(code < 0x81)
	{ // 正常字符
		// TODO: 注意输入的大小写的实现、不可打印字符的处理
		character=getChar(code);
		if(character!=0)
		{
			data=character|(0xc<<8);
			pos=(displayRow*80+displayCol)*2;
			bufferTail++;
			keyBuffer[bufferTail]=data;
			bufferTail=bufferTail%MAX_KEYBUFFER_SIZE;
			asm volatile("movw %0,(%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol==80)
			{
				displayCol=0;
				displayRow++;
			}
			if(displayRow==25)
			{
				scrollScreen();
				displayRow--;	
			}
		}
	}
	updateCursor(displayRow, displayCol);
}


void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel = USEL(SEG_UDATA);//TODO: segment selector for user data, need further modification
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) 
	{
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// TODO: 完成光标的维护和打印到显存
		if(character=='\n')
		{
			displayRow++;
			displayCol=0;
			if(displayRow==25)
			{
				scrollScreen();
				displayRow--;
			}
			data=character|(0xc<<8);
			pos=(displayRow*80+displayCol)*2;
		}	
		else
		{
			data=character|(0xc<<8);
			pos=(displayRow*80+displayCol)*2;
			asm volatile("movw %0,(%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol==80)
			{
				displayCol=0;
				displayRow++;
			}
			if(displayRow==25)
			{
				scrollScreen();
				displayRow--;
			}
		}
		
	}
	updateCursor(displayRow, displayCol);
}


void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
	int sel = USEL(SEG_UDATA);
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	bufferHead=bufferTail=0;
	while(1)
	{
		enableInterrupt();
		if(keyBuffer[bufferTail]==13)
		{
			keyBuffer[bufferTail]='\n';
			bufferTail--;
			disableInterrupt();
			break;
		}
	}
	asm volatile("movw %0, %%es"::"m"(sel));
	char character = keyBuffer[bufferHead];
	if(character)
		asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str));

}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
	int sel = USEL(SEG_UDATA);
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	bufferHead=bufferTail=0;
	while(1)
	{
		enableInterrupt();
		if(keyBuffer[bufferTail]==13)
		{
			keyBuffer[bufferTail]=0;
			disableInterrupt();
			break;
		}
	}
	asm volatile("movw %0, %%es"::"m"(sel));
	int i;
	for(i=0;i<size-1;)
	{
		if(bufferHead!=bufferTail)
		{
			char character = keyBuffer[bufferHead];
			bufferHead=(bufferHead+1)%MAX_KEYBUFFER_SIZE;
			if(character)
			{
				asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str));
				i++;
			}
		}
		else
			break;
	}
	asm volatile("movb $0x0, %%es:(%0)"::"r"(str+i));

}

