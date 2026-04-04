#include <stdio.h>
#include <sys.h>

#define FORK_PTE_BUFFER_SIZE 256

static void PrintPteFlags(unsigned short flags)
{
	printf("flags=%x[", flags);

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

	if ( (flags & (USER_PAGE_SNAPSHOT_PRESENT | USER_PAGE_SNAPSHOT_RW | USER_PAGE_SNAPSHOT_USER)) == 0 )
	{
		printf("none");
	}

	printf("]");
}

static void DumpCurrentProcessPageTable(const char* tag)
{
	struct user_page_snapshot_entry entries[FORK_PTE_BUFFER_SIZE];
	int total = getUserPageTable(entries, FORK_PTE_BUFFER_SIZE);
	int dumpCount;
	int i;

	if ( total < 0 )
	{
		printf("%s: getUserPageTable failed\n", tag);
		return;
	}

	dumpCount = total;
	if ( dumpCount > FORK_PTE_BUFFER_SIZE )
	{
		dumpCount = FORK_PTE_BUFFER_SIZE;
	}

	printf("%s: pid=%d total=%d dump=%d\n", tag, getpid(), total, dumpCount);
	for ( i = 0; i < dumpCount; ++i )
	{
		unsigned int va = ((unsigned int)entries[i].pageIndex) << 12;
		printf("  [%d] va=%x pfn=%x ",
			i,
			va,
			entries[i].pageBaseAddress);
		PrintPteFlags(entries[i].flags);
		printf("\n");
	}

	if ( total > FORK_PTE_BUFFER_SIZE )
	{
		printf("  ... truncated ...\n");
	}
}

int main1(int argc, char* argv[])
{
	int childPid;
	int status;
	int pid = getpid();

	(void)argc;
	(void)argv;

	printf("Before fork, pid=%d\n", pid);
	DumpCurrentProcessPageTable("before-fork");

	childPid = fork();
	if ( childPid < 0 )
	{
		printf("fork failed\n");
		exit(1);
	}

	if ( childPid == 0 )
	{
		printf("In child, pid=%d ppid=%d\n", getpid(), pid);
		DumpCurrentProcessPageTable("child-after-fork");
		exit(0);
	}

	wait(&status);
	printf("In parent, pid=%d child=%d\n", getpid(), childPid);
	DumpCurrentProcessPageTable("parent-after-fork");
	printf("Parent wait done, child status=%d\n", status);

	exit(0);
}
