#include <stdio.h>
#include <sys.h>

#define ZEROPAGE_PTE_BUFFER 256
#define ZEROPAGE_PAGE_SIZE 4096
#define USER_ZERO_PAGE_PFN 0x400

int g_bssProbe;
char g_bssPadding[64];

static unsigned long AlignUpToPage(unsigned long value)
{
	return (value + ZEROPAGE_PAGE_SIZE - 1) & ~(ZEROPAGE_PAGE_SIZE - 1);
}

static void PrintFlags(unsigned short flags)
{
	printf("[");
	if((flags & USER_PAGE_SNAPSHOT_PRESENT) != 0)
	{
		printf("P");
	}
	if((flags & USER_PAGE_SNAPSHOT_RW) != 0)
	{
		printf(" RW");
	}
	if((flags & USER_PAGE_SNAPSHOT_USER) != 0)
	{
		printf(" USR");
	}
	if((flags & USER_PAGE_SNAPSHOT_EXEC) != 0)
	{
		printf(" X");
	}
	printf(" ]");
}

static int FindPageEntry(unsigned long va, struct user_page_snapshot_entry* out)
{
	struct user_page_snapshot_entry entries[ZEROPAGE_PTE_BUFFER];
	int total = getUserPageTable(entries, ZEROPAGE_PTE_BUFFER);
	unsigned short pageIndex = (unsigned short)(va >> 12);
	int i;

	if(total < 0)
	{
		return -1;
	}
	if(total > ZEROPAGE_PTE_BUFFER)
	{
		total = ZEROPAGE_PTE_BUFFER;
	}

	for(i = 0; i < total; ++i)
	{
		if(entries[i].pageIndex == pageIndex)
		{
			*out = entries[i];
			return 0;
		}
	}

	return -1;
}

static void DumpAddressMapping(const char* tag,
							   const char* name,
							   void* address,
							   struct user_page_snapshot_entry* outEntry)
{
	struct user_page_snapshot_entry entry;
	unsigned long va = (unsigned long)address;

	if(FindPageEntry(va, &entry) == 0)
	{
		printf("%s pid=%d %s va=%x pfn=%x flags=%x",
			   tag,
			   getpid(),
			   name,
			   (unsigned int)va,
			   entry.pageBaseAddress,
			   entry.flags);
		PrintFlags(entry.flags);
		printf("\n");
		*outEntry = entry;
	}
	else
	{
		printf("%s pid=%d %s va=%x not-resident\n",
			   tag,
			   getpid(),
			   name,
			   (unsigned int)va);
		outEntry->pageBaseAddress = 0;
		outEntry->flags = 0;
	}
}

static void ValidateTransition(const char* name,
							   const struct user_page_snapshot_entry* beforeWrite,
							   const struct user_page_snapshot_entry* afterWrite,
							   int* pass)
{
	if(beforeWrite->pageBaseAddress != USER_ZERO_PAGE_PFN)
	{
		printf("check failed: %s before-write pfn expected=%x actual=%x\n",
			   name,
			   USER_ZERO_PAGE_PFN,
			   beforeWrite->pageBaseAddress);
		*pass = 0;
	}

	if((beforeWrite->flags & USER_PAGE_SNAPSHOT_RW) != 0)
	{
		printf("check failed: %s before-write should be readonly flags=%x\n",
			   name,
			   beforeWrite->flags);
		*pass = 0;
	}

	if((afterWrite->flags & USER_PAGE_SNAPSHOT_RW) == 0)
	{
		printf("check failed: %s after-write should be writable flags=%x\n",
			   name,
			   afterWrite->flags);
		*pass = 0;
	}

	if(afterWrite->pageBaseAddress == beforeWrite->pageBaseAddress)
	{
		printf("check failed: %s after-write pfn should differ from zero page pfn=%x\n",
			   name,
			   afterWrite->pageBaseAddress);
		*pass = 0;
	}
}

int main1()
{
	struct user_page_snapshot_entry heapBeforeWrite;
	struct user_page_snapshot_entry heapAfterWrite;
	struct user_page_snapshot_entry bssBeforeWrite;
	struct user_page_snapshot_entry bssAfterWrite;
	volatile unsigned char* probeAddress;
	unsigned long breakNow;
	unsigned long alignedBreak;
	unsigned long probePage;
	unsigned long newBreak;
	int growBytes;
	int heapBeforeValue;
	int bssBeforeValue;
	int pass = 1;

	bssBeforeValue = g_bssProbe;
	printf("bss-before-read pid=%d value=%d\n", getpid(), bssBeforeValue);
	DumpAddressMapping("bss-before-write", "g_bssProbe", (void*)&g_bssProbe, &bssBeforeWrite);

	g_bssProbe = 99;
	g_bssPadding[0] = 11;
	printf("bss-after-write pid=%d value=%d pad0=%d\n",
		   getpid(),
		   g_bssProbe,
		   (int)(unsigned char)g_bssPadding[0]);
	DumpAddressMapping("bss-after-write", "g_bssProbe", (void*)&g_bssProbe, &bssAfterWrite);

	/*
	 * 使用堆上“全新页”做测试，避免落到 data/bss 边界页导致误判。
	 * 注意：需放在 bss 验证之后，避免 sbrk 写 libc 的 _fakeedata 先触发 bss 页 COW。
	 */
	breakNow = (unsigned long)sbrk(0);
	alignedBreak = AlignUpToPage(breakNow);
	probePage = alignedBreak + ZEROPAGE_PAGE_SIZE;
	newBreak = probePage + ZEROPAGE_PAGE_SIZE;
	growBytes = (int)(newBreak - breakNow);
	if(growBytes <= 0 || sbrk(growBytes) <= 0)
	{
		printf("zeropage setup failed: brk grow error\n");
		exit(1);
	}
	probeAddress = (volatile unsigned char*)probePage;

	/* 首次读访问触发 BACKING_ZERO 缺页，映射到共享零页。 */
	heapBeforeValue = probeAddress[0];
	printf("heap-before-read pid=%d value=%d\n", getpid(), heapBeforeValue);
	DumpAddressMapping("heap-before-write", "heapProbe", (void*)probeAddress, &heapBeforeWrite);

	/* 首次写访问触发 COW 分裂，获得私有可写页。 */
	probeAddress[0] = (unsigned char)(2026 & 0xFF);
	probeAddress[128] = 7;
	printf("heap-after-write pid=%d value=%d pad0=%d\n",
		   getpid(),
		   (int)(unsigned char)probeAddress[0],
		   (int)(unsigned char)probeAddress[128]);
	DumpAddressMapping("heap-after-write", "heapProbe", (void*)probeAddress, &heapAfterWrite);

	ValidateTransition("heapProbe", &heapBeforeWrite, &heapAfterWrite, &pass);
	ValidateTransition("g_bssProbe", &bssBeforeWrite, &bssAfterWrite, &pass);

	if(pass)
	{
		printf("zeropage PASS\n");
		exit(0);
	}

	printf("zeropage FAIL\n");
	exit(1);
}