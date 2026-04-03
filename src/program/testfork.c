#include <stdio.h>
#include <sys.h>

int globalVar = 1;

int main1()
{
	int i = 1;
	printf("Before fork, i = %d, globalVar = %d\n", i, globalVar);
	if ( fork() == 0 )
	{
		i = 2;
		globalVar = 2;
		printf("In child process, i = %d, globalVar = %d\n", i, globalVar);
	}
	else
	{
		i = 3;
		globalVar = 3;
		printf("In parent process, i = %d, globalVar = %d\n", i, globalVar);
	}
}