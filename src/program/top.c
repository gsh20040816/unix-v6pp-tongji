#include <stdio.h>
#include <sys.h>

int main1(int argc, char* argv[])
{
	int userFree;
	int kernelHeapFree;
	int kernelPageFree;

	(void)argc;
	(void)argv;

	userFree = getUserFreeMemory();
	kernelHeapFree = getKernelHeapFreeMemory();
	kernelPageFree = getKernelPageFreeMemory();

	if(userFree < 0 || kernelHeapFree < 0 || kernelPageFree < 0)
	{
		printf("top: query memory failed\n");
		return 1;
	}

	printf("top: user free=%d bytes (%d KB)\n", userFree, userFree / 1024);
	printf("top: kernel heap free=%d bytes (%d KB)\n",
		   kernelHeapFree,
		   kernelHeapFree / 1024);
	printf("top: kernel page free=%d bytes (%d KB)\n",
		   kernelPageFree,
		   kernelPageFree / 1024);

	return 0;
}
