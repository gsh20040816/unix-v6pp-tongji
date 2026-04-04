#include <stdio.h>
#include <sys.h>

int main1(int argc, char* argv[])
{
	if ( -1 == syncFileSystem() )
		printf("Error Happens During File System Update Progress...:(\n");
	else
		printf("File System Successfully Updated!\n");

	printf("Requesting power off...\n");
	if ( -1 == shutdownSystem() )
	{
		printf("Power off request failed.\n");
	}
	
	return 1;
}
