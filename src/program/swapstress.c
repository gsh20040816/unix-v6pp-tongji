#include <stdio.h>
#include <sys.h>

#define SWAPSTRESS_PAGE_SIZE 4096
#define SWAPSTRESS_CHILDREN 8
#define SWAPSTRESS_PAGES_PER_CHILD 768
#define SWAPSTRESS_STEP_PAGES 64
#define SWAPSTRESS_PTE_BUFFER 1024
#define SWAPSTRESS_HOLD_SECONDS 10
#define SWAPSTRESS_EXIT_PASS_SWAP 0
#define SWAPSTRESS_EXIT_FAIL 1
#define SWAPSTRESS_EXIT_PASS_NO_SWAP 2

static struct user_page_snapshot_entry g_pageEntries[SWAPSTRESS_PTE_BUFFER];
static unsigned char g_residentHeapPages[SWAPSTRESS_PAGES_PER_CHILD];

static unsigned long AlignUpToPage(unsigned long value)
{
	return (value + SWAPSTRESS_PAGE_SIZE - 1) & ~(SWAPSTRESS_PAGE_SIZE - 1);
}

static void TouchSnapshotBuffer()
{
	volatile unsigned char* bytes = (volatile unsigned char*)g_pageEntries;
	unsigned int i;

	for(i = 0; i < sizeof(g_pageEntries); i += SWAPSTRESS_PAGE_SIZE)
	{
		bytes[i] = 0;
	}
	bytes[sizeof(g_pageEntries) - 1] = 0;
}

static int ResidentPageCount()
{
	int total;

	TouchSnapshotBuffer();
	total = getUserPageTable(g_pageEntries, SWAPSTRESS_PTE_BUFFER);
	if(total < 0)
	{
		return -1;
	}
	return total;
}

static int MarkResidentHeapPages(unsigned long start, int pageCount)
{
	unsigned short startPage = (unsigned short)(start >> 12);
	unsigned short endPage = (unsigned short)(startPage + pageCount);
	int total;
	int limit;
	int i;
	int resident = 0;

	if(pageCount > SWAPSTRESS_PAGES_PER_CHILD)
	{
		return -1;
	}

	for(i = 0; i < pageCount; ++i)
	{
		g_residentHeapPages[i] = 0;
	}

	TouchSnapshotBuffer();
	total = getUserPageTable(g_pageEntries, SWAPSTRESS_PTE_BUFFER);
	if(total < 0)
	{
		return -1;
	}

	limit = total;
	if(limit > SWAPSTRESS_PTE_BUFFER)
	{
		limit = SWAPSTRESS_PTE_BUFFER;
	}

	for(i = 0; i < limit; ++i)
	{
		unsigned short pageIndex = g_pageEntries[i].pageIndex;
		if(pageIndex >= startPage && pageIndex < endPage)
		{
			int heapIndex = pageIndex - startPage;
			if(g_residentHeapPages[heapIndex] == 0)
			{
				g_residentHeapPages[heapIndex] = 1;
				++resident;
			}
		}
	}

	return resident;
}

static unsigned char ExpectedByte(int childIndex, int pageIndex, int offset)
{
	unsigned int value = 0x9e3779b9U;

	value ^= (unsigned int)(childIndex + 1) * 0x45d9f3bU;
	value ^= (unsigned int)(pageIndex + 1) * 0x119de1f3U;
	value ^= (unsigned int)(offset + 1) * 0x27d4eb2dU;
	value ^= value >> 16;
	return (unsigned char)(value & 0xff);
}

static unsigned int MixChecksum(unsigned int checksum, unsigned char value)
{
	return (checksum << 5) - checksum + value;
}

static unsigned int FillPage(volatile unsigned char* page,
							 int childIndex,
							 int pageIndex)
{
	unsigned int checksum = 0;
	int offset;

	for(offset = 0; offset < SWAPSTRESS_PAGE_SIZE; ++offset)
	{
		unsigned char value = ExpectedByte(childIndex, pageIndex, offset);
		page[offset] = value;
		checksum = MixChecksum(checksum, value);
	}

	return checksum;
}

static int VerifyPage(volatile unsigned char* page,
					  int childIndex,
					  int pageIndex,
					  unsigned int* checksum)
{
	int offset;
	unsigned int pageChecksum = 0;

	for(offset = 0; offset < SWAPSTRESS_PAGE_SIZE; ++offset)
	{
		unsigned char actual = page[offset];
		unsigned char expected = ExpectedByte(childIndex, pageIndex, offset);
		pageChecksum = MixChecksum(pageChecksum, actual);
		if(actual != expected)
		{
			printf("swapstress: child=%d data mismatch page=%d offset=%d expected=%d actual=%d\n",
				   childIndex,
				   pageIndex,
				   offset,
				   (int)expected,
				   (int)actual);
			return 0;
		}
	}

	*checksum += pageChecksum;
	return 1;
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
	int afterTouchResidentHeap;
	int beforeVerifyResidentHeap;
	int afterVerifyResidentHeap;
	int swappedBeforeVerify;
	int verifiedSwapped = 0;
	int verifiedResident = 0;
	unsigned int writeChecksum = 0;
	unsigned int readChecksum = 0;

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
		writeChecksum += FillPage(page, childIndex, i);
		++pages;

		if((pages % SWAPSTRESS_STEP_PAGES) == 0)
		{
			printf("swapstress: child=%d touched=%d free=%d resident=%d checksum=%x\n",
				   childIndex,
				   pages,
				   getUserFreeMemory(),
				   ResidentPageCount(),
				   writeChecksum);
		}
	}

	afterTouchFree = getUserFreeMemory();
	afterResident = ResidentPageCount();
	afterTouchResidentHeap = MarkResidentHeapPages(start, pages);
	if(afterResident < 0 || afterTouchResidentHeap < 0)
	{
		printf("swapstress: child=%d page snapshot failed\n", childIndex);
		exit(SWAPSTRESS_EXIT_FAIL);
	}

	printf("swapstress: child=%d touched pages=%d heap-resident=%d heap-nonresident=%d free-after-grow=%d free-after-touch=%d resident-after=%d checksum=%x\n",
		   childIndex,
		   pages,
		   afterTouchResidentHeap,
		   pages - afterTouchResidentHeap,
		   afterGrowFree,
		   afterTouchFree,
		   afterResident,
		   writeChecksum);
	printf("swapstress: child=%d holding pages for %d seconds\n",
		   childIndex,
		   SWAPSTRESS_HOLD_SECONDS);
	sleep(SWAPSTRESS_HOLD_SECONDS);

	beforeVerifyResidentHeap = MarkResidentHeapPages(start, pages);
	if(beforeVerifyResidentHeap < 0)
	{
		printf("swapstress: child=%d page snapshot before verify failed\n",
			   childIndex);
		exit(SWAPSTRESS_EXIT_FAIL);
	}
	swappedBeforeVerify = pages - beforeVerifyResidentHeap;

	for(i = 0; i < pages; ++i)
	{
		if(g_residentHeapPages[i] == 0)
		{
			volatile unsigned char* page =
				base + (unsigned long)i * SWAPSTRESS_PAGE_SIZE;
			if(VerifyPage(page, childIndex, i, &readChecksum) == 0)
			{
				printf("swapstress: child=%d FAIL swapped-page-verify page=%d\n",
					   childIndex,
					   i);
				exit(SWAPSTRESS_EXIT_FAIL);
			}
			++verifiedSwapped;
		}
	}

	for(i = 0; i < pages; ++i)
	{
		if(g_residentHeapPages[i] != 0)
		{
			volatile unsigned char* page =
				base + (unsigned long)i * SWAPSTRESS_PAGE_SIZE;
			if(VerifyPage(page, childIndex, i, &readChecksum) == 0)
			{
				printf("swapstress: child=%d FAIL resident-page-verify page=%d\n",
					   childIndex,
					   i);
				exit(SWAPSTRESS_EXIT_FAIL);
			}
			++verifiedResident;
		}
	}

	afterVerifyResidentHeap = MarkResidentHeapPages(start, pages);
	if(afterVerifyResidentHeap < 0)
	{
		printf("swapstress: child=%d page snapshot after verify failed\n",
			   childIndex);
		exit(SWAPSTRESS_EXIT_FAIL);
	}

	if(readChecksum != writeChecksum)
	{
		printf("swapstress: child=%d FAIL checksum mismatch write=%x read=%x\n",
			   childIndex,
			   writeChecksum,
			   readChecksum);
		exit(SWAPSTRESS_EXIT_FAIL);
	}

	printf("swapstress: child=%d verify swapped-before=%d verified-swapped=%d verified-resident=%d resident-after-verify=%d write-checksum=%x read-checksum=%x\n",
		   childIndex,
		   swappedBeforeVerify,
		   verifiedSwapped,
		   verifiedResident,
		   afterVerifyResidentHeap,
		   writeChecksum,
		   readChecksum);

	if(verifiedSwapped > 0)
	{
		printf("swapstress: child=%d PASS swap-readback\n", childIndex);
		exit(SWAPSTRESS_EXIT_PASS_SWAP);
	}

	printf("swapstress: child=%d PASS no-local-swap\n", childIndex);
	exit(SWAPSTRESS_EXIT_PASS_NO_SWAP);
}

int main1()
{
	int child;
	int status = 0;
	int i;
	int failed = 0;
	int sawSwapReadback = 0;
	int verifiedChildren = 0;
	int noLocalSwapChildren = 0;

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
		if(status == SWAPSTRESS_EXIT_PASS_SWAP)
		{
			sawSwapReadback = 1;
			++verifiedChildren;
		}
		else if(status == SWAPSTRESS_EXIT_PASS_NO_SWAP)
		{
			++noLocalSwapChildren;
			++verifiedChildren;
		}
		else
		{
			failed = 1;
		}
	}

	if(failed || sawSwapReadback == 0)
	{
		printf("swapstress: FAIL swap-observed=%d children-verified=%d no-local-swap=%d free-end=%d\n",
			   sawSwapReadback,
			   verifiedChildren,
			   noLocalSwapChildren,
			   getUserFreeMemory());
		exit(1);
	}

	printf("swapstress: PASS swap-observed=1 children-verified=%d no-local-swap=%d free-end=%d\n",
		   verifiedChildren,
		   noLocalSwapChildren,
		   getUserFreeMemory());
	printf("swapstress: done free-end=%d\n", getUserFreeMemory());
	exit(0);
}
