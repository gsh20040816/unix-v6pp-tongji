#include <stdio.h>
#include <stdlib.h>

int main1(int argc, char* argv[])
{
	int i;

	printf("argc=%d\n", argc);
	for(i = 0; i < argc; i++)
	{
		if(argv[i])
		{
			printf("argv[%d]='%s'\n", i, argv[i]);
		}
		else
		{
			printf("argv[%d]=<null>\n", i);
		}
	}

	exit(0);
}
