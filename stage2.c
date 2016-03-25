#include "types.h"
#include "user.h"
#include "signal.h"

volatile int flag = 0;

void dummy(void)
{
    printf(1, "TEST FAILED: this should never execute.\n");
}

void handle_signal(siginfo_t info)
{
    __asm__ ("movl $0x0,%ecx\n\t");
    flag = 1;
} 

int main(void)
{
    register int eax asm ("%eax");
    register int ecx asm ("%ecx");
    register int edx asm ("%edx");
    
    signal(SIGALRM, handle_signal);

    eax = 3;
    ecx = 5;
    edx = 7;

    alarm(1);

    while(!flag);

    if (ecx == 5)
        printf(1, "TEST PASSED: Final value of eax is %d, ecx is %d, edx is %d...\n", eax, ecx, edx);
    else
        printf(1, "TEST FAILED: Final value of eax is %d, ecx is %d, edx is %d...\n", eax, ecx, edx);

    exit();
}
