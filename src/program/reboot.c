#include <stdio.h>
#include <sys.h>

int main1(int argc, char* argv[])
{
	if ( -1 == syncFileSystem() )
	{
		printf("sync failed before reboot\n");
	}

	printf("Requesting reboot...\n");
	if ( -1 == rebootSystem() )
	{
		printf("Reboot request failed.\n");
	}

	return 1;
}
