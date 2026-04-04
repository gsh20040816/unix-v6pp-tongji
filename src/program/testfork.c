#include <stdio.h>
#include <sys.h>

#define TESTFORK_PTE_BUFFER 256
#define TESTFORK_PAGE_SIZE 4096

int globalVar = 1;
char globalPages[TESTFORK_PAGE_SIZE * 2];

static void PrintFlags(unsigned short flags)
{
	printf("[");
	if ( (flags & USER_PAGE_SNAPSHOT_PRESENT) != 0 )
	{
		printf("P");
	}
	if ( (flags & USER_PAGE_SNAPSHOT_RW) != 0 )
	{
		printf(" RW");
	}
	if ( (flags & USER_PAGE_SNAPSHOT_USER) != 0 )
	{
		printf(" USR");
	}
	if ( (flags & USER_PAGE_SNAPSHOT_EXEC) != 0 )
	{
		printf(" X");
	}
	printf(" ]");
}

static int FindPageEntry(unsigned int va,
	struct user_page_snapshot_entry* out)
{
	struct user_page_snapshot_entry entries[TESTFORK_PTE_BUFFER];
	int total = getUserPageTable(entries, TESTFORK_PTE_BUFFER);
	unsigned short pageIndex = (unsigned short)(va >> 12);
	int i;

	if ( total < 0 )
	{
		return -1;
	}

	if ( total > TESTFORK_PTE_BUFFER )
	{
		total = TESTFORK_PTE_BUFFER;
	}

	for ( i = 0; i < total; ++i )
	{
		if ( entries[i].pageIndex == pageIndex )
		{
			*out = entries[i];
			return 0;
		}
	}

	return -1;
}

static void DumpAddressMapping(const char* tag,
	const char* name,
	void* address)
{
	struct user_page_snapshot_entry entry;
	unsigned int va = (unsigned int)address;
	if ( FindPageEntry(va, &entry) == 0 )
	{
		printf("%s pid=%d %s va=%x pfn=%x flags=%x",
			tag,
			getpid(),
			name,
			va,
			entry.pageBaseAddress,
			entry.flags);
		PrintFlags(entry.flags);
		printf("\n");
	}
	else
	{
		printf("%s pid=%d %s va=%x not-resident\n", tag, getpid(), name, va);
	}
}

static void TouchWritablePages()
{
	globalPages[0] = 7;
	globalPages[TESTFORK_PAGE_SIZE] = 9;
}

int main1()
{
	int stackVar = 11;
	int child;
	int status = 0;

	TouchWritablePages();
	printf("before-fork pid=%d global=%d stack=%d p0=%d p1=%d\n",
		getpid(),
		globalVar,
		stackVar,
		(int)(unsigned char)globalPages[0],
		(int)(unsigned char)globalPages[TESTFORK_PAGE_SIZE]);
	DumpAddressMapping("before-fork", "globalVar", (void*)&globalVar);
	DumpAddressMapping("before-fork", "stackVar", (void*)&stackVar);

	child = fork();
	if ( child < 0 )
	{
		printf("fork failed\n");
		exit(1);
	}

	if ( child == 0 )
	{
		DumpAddressMapping("child-before-write", "globalVar", (void*)&globalVar);
		DumpAddressMapping("child-before-write", "stackVar", (void*)&stackVar);

		globalVar += 100;
		stackVar += 200;
		globalPages[0] += 3;
		globalPages[TESTFORK_PAGE_SIZE] += 5;

		printf("child-after-write pid=%d global=%d stack=%d p0=%d p1=%d\n",
			getpid(),
			globalVar,
			stackVar,
			(int)(unsigned char)globalPages[0],
			(int)(unsigned char)globalPages[TESTFORK_PAGE_SIZE]);
		DumpAddressMapping("child-after-write", "globalVar", (void*)&globalVar);
		DumpAddressMapping("child-after-write", "stackVar", (void*)&stackVar);
		exit(0);
	}

	wait(&status);
	printf("parent-after-wait pid=%d child=%d status=%d global=%d stack=%d p0=%d p1=%d\n",
		getpid(),
		child,
		status,
		globalVar,
		stackVar,
		(int)(unsigned char)globalPages[0],
		(int)(unsigned char)globalPages[TESTFORK_PAGE_SIZE]);
	DumpAddressMapping("parent-after-wait", "globalVar", (void*)&globalVar);
	DumpAddressMapping("parent-after-wait", "stackVar", (void*)&stackVar);

	exit(0);
}