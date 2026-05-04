#include <stdio.h>
#include <sys.h>

#define SWAPSTRESS_PAGE_SIZE 4096
#define SWAPSTRESS_CHILDREN 10
#define SWAPSTRESS_PAGES_PER_CHILD 640
#define SWAPSTRESS_STEP_PAGES 64
#define SWAPSTRESS_PTE_BUFFER 256
#define SWAPSTRESS_HOLD_SECONDS 10

static unsigned long AlignUpToPage(unsigned long value)
{
	return (value + SWAPSTRESS_PAGE_SIZE - 1) & ~(SWAPSTRESS_PAGE_SIZE - 1);
}

static int ResidentPageCount()
{
	struct user_page_snapshot_entry entries[SWAPSTRESS_PTE_BUFFER];
	int total = getUserPageTable(entries, SWAPSTRESS_PTE_BUFFER);
	if(total < 0)
	{
		return -1;
	}
	return total;
}

static void RunChild(int childIndex)
{
	volatile unsigned char* base;
	unsigned long breakNow;
	unsigned long start;
	unsigned long targetBreak;
	int growBytes;
	int pages = 0;
	int i;
	int afterGrowFree;
	int afterTouchFree;
	int afterResident;
	unsigned int checksum = 0;

	breakNow = (unsigned long)sbrk(0);
	start = AlignUpToPage(breakNow);
	targetBreak = start +
		(unsigned long)SWAPSTRESS_PAGES_PER_CHILD * SWAPSTRESS_PAGE_SIZE;
	growBytes = (int)(targetBreak - breakNow);

	printf("swapstress: child=%d pid=%d brk=%x target=%x free-before=%d resident-before=%d\n",
		   childIndex,
		   getpid(),
		   (unsigned int)breakNow,
		   (unsigned int)targetBreak,
		   getUserFreeMemory(),
		   ResidentPageCount());

	if(growBytes <= 0 || brk((void*)targetBreak) < 0)
	{
		printf("swapstress: child=%d brk failed grow=%d target=%x\n",
			   childIndex,
			   growBytes,
			   (unsigned int)targetBreak);
		exit(1);
	}

	afterGrowFree = getUserFreeMemory();
	base = (volatile unsigned char*)start;

	for(i = 0; i < SWAPSTRESS_PAGES_PER_CHILD; ++i)
	{
		volatile unsigned char* page = base + (unsigned long)i * SWAPSTRESS_PAGE_SIZE;
		page[0] = (unsigned char)((childIndex + i) & 0xff);
		page[SWAPSTRESS_PAGE_SIZE - 1] =
			(unsigned char)(((childIndex + 1) * 17 + i) & 0xff);
		checksum += page[0];
		checksum += page[SWAPSTRESS_PAGE_SIZE - 1];
		++pages;

		if((pages % SWAPSTRESS_STEP_PAGES) == 0)
		{
			printf("swapstress: child=%d touched=%d free=%d resident=%d checksum=%x\n",
				   childIndex,
				   pages,
				   getUserFreeMemory(),
				   ResidentPageCount(),
				   checksum);
		}
	}

	for(i = 0; i < pages; i += 31)
	{
		volatile unsigned char* page = base + (unsigned long)i * SWAPSTRESS_PAGE_SIZE;
		checksum += page[0];
		checksum += page[SWAPSTRESS_PAGE_SIZE - 1];
	}

	afterTouchFree = getUserFreeMemory();
	afterResident = ResidentPageCount();
	printf("swapstress: child=%d done pages=%d free-after-grow=%d free-after-touch=%d resident-after=%d checksum=%x\n",
		   childIndex,
		   pages,
		   afterGrowFree,
		   afterTouchFree,
		   afterResident,
		   checksum);
	printf("swapstress: child=%d holding pages for %d seconds\n",
		   childIndex,
		   SWAPSTRESS_HOLD_SECONDS);
	sleep(SWAPSTRESS_HOLD_SECONDS);
	exit(0);
}

int main1()
{
	int child;
	int status = 0;
	int i;
	int failed = 0;

	printf("swapstress: parent pid=%d children=%d pages-per-child=%d free-start=%d\n",
		   getpid(),
		   SWAPSTRESS_CHILDREN,
		   SWAPSTRESS_PAGES_PER_CHILD,
		   getUserFreeMemory());

	for(i = 0; i < SWAPSTRESS_CHILDREN; ++i)
	{
		child = fork();
		if(child < 0)
		{
			printf("swapstress: fork failed at child=%d\n", i);
			failed = 1;
			break;
		}

		if(child == 0)
		{
			RunChild(i);
		}
	}

	while(wait(&status) > 0)
	{
		printf("swapstress: child exited status=%d free-now=%d\n",
			   status,
			   getUserFreeMemory());
		if(status != 0)
		{
			failed = 1;
		}
	}

	if(failed)
	{
		printf("swapstress: FAIL free-end=%d\n", getUserFreeMemory());
		exit(1);
	}

	printf("swapstress: done free-end=%d\n", getUserFreeMemory());
	exit(0);
}
