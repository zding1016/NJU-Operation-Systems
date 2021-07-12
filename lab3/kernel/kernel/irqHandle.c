#include "x86.h"
#include "device.h"


extern int displayRow;
extern int displayCol;
extern int current;
extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM]; // pcb

void GProtectFaultHandle(struct StackFrame *sf);

void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallPrint(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);

void irqHandle(struct StackFrame *sf) { // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	/*TODO Save esp to stackTop */
	uint32_t temp=pcb[current].stackTop;
	pcb[current].prevStackTop=pcb[current].stackTop;
	pcb[current].stackTop=(uint32_t)sf;

	switch(sf->irq) {
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(sf);
			break;
		case 0x20:
			timerHandle(sf);
			break;
		case 0x80:
			syscallHandle(sf);
			break;
		default:assert(0);
	}
	/*TODO Recover stackTop */
	pcb[current].stackTop=temp;
}

void GProtectFaultHandle(struct StackFrame *sf) {
	assert(0);
	return;
}


void syscallHandle(struct StackFrame *sf) {
	switch(sf->eax) { // syscall number
		case 0:
			syscallWrite(sf);
			break; // for SYS_WRITE
		/*TODO Add Fork,Sleep... */
		case 1:
			syscallFork(sf);
			break;
		case 3:
			syscallSleep(sf);
			break;
		case 4:
			syscallExit(sf);
			break;
		default:break;
	}
}

void syscallWrite(struct StackFrame *sf) {
	switch(sf->ecx) { // file descriptor
		case 0:
			syscallPrint(sf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct StackFrame *sf) {
	int sel = sf->ds; //TODO segment selector for user data, need further modification
	char *str = (char*)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		if(character == '\n') {
			displayRow++;
			displayCol=0;
			if(displayRow==25){
				displayRow=24;
				displayCol=0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol==80){
				displayRow++;
				displayCol=0;
				if(displayRow==25){
					displayRow=24;
					displayCol=0;
					scrollScreen();
				}
			}
		}
		//asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
		//asm volatile("int $0x20":::"memory"); //XXX Testing irqTimer during syscall
	}
	
	updateCursor(displayRow, displayCol);
	//TODO take care of return value
	return;
}
void timerHandle(struct StackFrame *sf)
{
	for(int i=0;i<MAX_PCB_NUM;i++)
	{
		if(pcb[i].state==STATE_BLOCKED)
		{	pcb[i].sleepTime--;
			if(pcb[i].sleepTime<=0)
				pcb[i].state=STATE_RUNNABLE;
		}
	}
	if(pcb[current].state==STATE_RUNNING)
	{	
		pcb[current].timeCount++;
		if(pcb[current].timeCount<=MAX_TIME_COUNT)
			return;
	}
	int i;
	for(i=(current+1)%MAX_PCB_NUM;i!=current;i=(i+1)%MAX_PCB_NUM)
	{
		if(pcb[i].state==STATE_RUNNABLE)
			break;
	}
	if(pcb[current].state==STATE_RUNNING)
	{
		pcb[current].state=STATE_RUNNABLE;
		pcb[current].timeCount=0;
	}
	current=i;
	pcb[current].state=STATE_RUNNING;
	pcb[current].timeCount=0;
	uint32_t temp=pcb[current].stackTop;
	pcb[current].stackTop=pcb[current].prevStackTop;
	tss.esp0=(uint32_t)&(pcb[current].stackTop);
	asm volatile("movl %0, %%esp"::"m"(temp)); // switch kernel stack
	asm volatile("popl %gs");
	asm volatile("popl %fs");
	asm volatile("popl %es");
	asm volatile("popl %ds");
	asm volatile("popal");
	asm volatile("addl $8, %esp");
	asm volatile("iret");	

}
void syscallFork(struct StackFrame *sf)
{
	int i;
	int flag=0;
	for(i=0;i<MAX_PCB_NUM;i++)
	{
		if(pcb[i].state==STATE_DEAD)
		{
			flag=1;
			break;
		}
	}
	if(flag)
	{
		//int cnt=0;
		//enableInterrupt();
		for(int j=0;j<0x100000;j++)
		{
			//cnt++;
			*(uint8_t*)(j+(i+1)*0x100000)=*(uint8_t*)(j+(current+1)*0x100000);
			/*if(cnt==10000)
			{
				asm volatile("int $0x20");
				cnt=0;
			}*/
		}
		//disableInterrupt();
		pcb[i].stackTop = (uint32_t)&(pcb[i].regs);
		pcb[i].prevStackTop = (uint32_t)&(pcb[i].stackTop);
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = 0;
		pcb[i].sleepTime = 0;
		pcb[i].pid = i;
		pcb[i].regs.cs = USEL(1+2*i);
		pcb[i].regs.ss = USEL(2+2*i);
		pcb[i].regs.ds = USEL(2+2*i);
		pcb[i].regs.es = USEL(2+2*i);
		pcb[i].regs.fs = USEL(2+2*i);
		pcb[i].regs.gs = USEL(2+2*i);
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags | 0x200;
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = 0;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[current].regs.eax=i;	
	}
	else
		pcb[current].regs.eax=-1;
}
void syscallSleep(struct StackFrame *sf)
{
	if(sf->ecx<=0)
		return;
	else
	{
		pcb[current].sleepTime=sf->ecx;
		pcb[current].state=STATE_BLOCKED;
		asm volatile("int $0x20");
	}

}
void syscallExit(struct StackFrame *sf)
{
	pcb[current].state=STATE_DEAD;
	asm volatile("int $0x20");
}

