#include <stdio.h>
#include <unistd.h>
int gval=10;

int main(int argc, char *argv[])
{
	int lval = 20;
	pid_t pid;
	
	lval += 5;
	gval++;
	
	pid = fork();		
	if (pid == 0)	// if Child Process
		gval++;
	else			// if Parent Process
		lval++;
	
	if (pid == 0)
		printf("Child Proc: [%d, %d] \n", gval, lval);
	else
		printf("Parent Proc: [%d, %d] \n", gval, lval);
	return 0;
}