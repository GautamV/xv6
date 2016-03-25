#include "types.h"
#include "stat.h"
#include "user.h"
#include "signal.h"

static int start;
static int end;
static int traps;

void dummy(void){}

void handle_signal(int signum)
{
	traps++;
	//printf(1, "%d\n", traps);
	if (traps == 10000000){
		end = uptime();
		stop();	
	}
}

int main(int argc, char *argv[])
{
	traps = 0;
	start = uptime();
	signal(SIGFPE, handle_signal);
	int x = 3/0;

	printf(1, "Traps Performed: %d\n", traps);
	printf(1, "Total Elapsed Time: %d milliseconds\n", 10*(end - start));
	printf(1, "Average Time Per Trap: %d nanoseconds\n", end - start);

	exit();
}
