#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "signal.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;


void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
  
  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

void callUserHandler(uint sighandler) 
{ 
// char* addr = uva2ka(proc->pgdir, (char*)proc->tf->esp); 
// if ((proc->tf->esp & 0xFFF) == 0) 
// panic("esp_offset == 0");  
// *(int*)(addr + ((proc->tf->esp - 4) & 0xFFF)) = proc->tf->eip;

} 
/*void 
trampoline(void)
{
__asm__ (
	"trampoline: \n\t"
	"movl $0, 4(%%eax)\t #Put 0=SIGFPE on stack\n"
	"movl %2, 8(%%eax)\t #Put edx on stack\n"
	"movl %3, 12(%%eax)\t #Put ecx on stack\n"
	"movl %4, 16(%%eax)\t #Put eax on stack\n"
	"movl %5, 20(%%eax)\t #Put old eip on stack\n"
	"addl $24, %%eax\t #Expand stack \n"
	:  : "r" (old_esp), "r" (restorer), "r" (old_edx), "r" (old_ecx), "r" (old_eax), "r" (old_eip));

}*/


//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
int i;
struct proc *p;
//cprintf("trap occurs with trapno %d, T_DIVIDE is %d\n", tf->trapno, T_DIVIDE);
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit();
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_DIVIDE: 
    cprintf("Got to the divide trap - value of sigfpe handler is %d\n", proc->sighandlers[0]);
    if (proc->sighandlers[0] >= 0) {
	//add registers to stack 
	*((int*)(proc->tf->esp - 4)) = proc->tf->eip; 
	*((int*)(proc->tf->esp - 8)) = proc->tf->eax;
	*((int*)(proc->tf->esp - 12)) = proc->tf->ecx;
	*((int*)(proc->tf->esp - 16)) = proc->tf->edx;
	//add handler arg to stack and set eip
	 struct siginfo_t info;
	info.signum = SIGFPE;
	*((siginfo_t*)(proc->tf->esp - 20)) = info;
	*((int*)(proc->tf->esp - 24)) = proc->tramp;
	cprintf("&info is %d, info is %d, info.signum is %d\n", &info, info, info.signum);
	cprintf("tramp is %d\n", proc->tramp);
	 proc->tf->esp -= 24;  
	 proc->tf->eip = (uint) proc->sighandlers[0]; 
        return;
    }

    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpu->id, tf->eip, 
            rcr2());
    proc->killed = 1;

    if (proc->killed)
	exit();
    return;
  case T_IRQ0 + IRQ_TIMER:
	
	for (i = 0; i < 64; i++){
	p = (struct proc*) getproc(i);	
	if (p && p->alarmtime > 0) {
		p->alarmcounter++; 
		//cprintf("alarmtime is %d, counter is %d", proc->alarmtime, proc->alarmcounter);
		if (p->alarmcounter >= p->alarmtime){
			p->alarmtime = 0;
			p->alarmcounter = 0;
			//callUserHandler(proc->sighandlers[1]);
			if (p->sighandlers[1] >= 0) {
			//add registers to stack 
			*((int*)(proc->tf->esp - 4)) = proc->tf->eip; 
			*((int*)(proc->tf->esp - 8)) = proc->tf->eax;
			*((int*)(proc->tf->esp - 12)) = proc->tf->ecx;
			*((int*)(proc->tf->esp - 16)) = proc->tf->edx;
			struct siginfo_t info;			
			info.signum = SIGALRM;
			*((siginfo_t*)(proc->tf->esp - 20)) = info;
			*((int*)(proc->tf->esp - 24)) = proc->tramp;
			cprintf("&info is %d, info is %d, info.signum is %d", &info, info, info.signum);
	 		p->tf->esp -= 24;  
	 		p->tf->eip = (uint) p->sighandlers[1]; }
		}		
	}
	}	

    if(cpu->id == 0){		
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpu->id, tf->cs, tf->eip);
    lapiceoi();
    break;
   
  //PAGEBREAK: 13
  default:
    if(proc == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpu->id, tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpu->id, tf->eip, 
            rcr2());
    proc->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running 
  // until it gets to the regular system call return.)
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
}
