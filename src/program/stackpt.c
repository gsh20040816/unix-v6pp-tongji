#include <stdio.h>
#include <sys.h>

#define STACK_PTE_BUFFER_SIZE 256
#define STACK_TOUCH_WORDS 256

static volatile unsigned int g_stackChecksum = 0;

static int ParsePositiveInt(const char* text)
{
	int value = 0;
	int i = 0;

	if(text == 0)
	{
		return 0;
	}

	if(text[0] == '+')
	{
		i = 1;
	}

	for(; text[i] != '\0'; ++i)
	{
		if(text[i] < '0' || text[i] > '9')
		{
			break;
		}
		value = value * 10 + (text[i] - '0');
	}

	return value;
}

static void TouchStackRecursively(int depth)
{
	volatile unsigned int local[STACK_TOUCH_WORDS];
	int i;

	for(i = 0; i < STACK_TOUCH_WORDS; ++i)
	{
		local[i] = (unsigned int)(depth + i);
	}
	g_stackChecksum += local[depth & (STACK_TOUCH_WORDS - 1)];

	if(depth > 0)
	{
		TouchStackRecursively(depth - 1);
	}
}

static void DumpUserPageTable(const char* tag)
{
	struct user_page_snapshot_entry entries[STACK_PTE_BUFFER_SIZE];
	int total = getUserPageTable(entries, STACK_PTE_BUFFER_SIZE);
	int i;
	int dumpCount;

	if(total < 0)
	{
		printf("%s: getUserPageTable failed\n", tag);
		return;
	}

	dumpCount = total;
	if(dumpCount > STACK_PTE_BUFFER_SIZE)
	{
		dumpCount = STACK_PTE_BUFFER_SIZE;
	}

	printf("%s: total=%d dump=%d\n", tag, total, dumpCount);
	for(i = 0; i < dumpCount; ++i)
	{
		unsigned int va = ((unsigned int)entries[i].pageIndex) << 12;
		printf("[%d] va=%x pfn=%x flags=%x\n",
			   i, va, entries[i].pageBaseAddress, entries[i].flags);
	}

	if(total > STACK_PTE_BUFFER_SIZE)
	{
		printf("... truncated ...\n");
	}
}

int main1(int argc, char* argv[])
{
	int depth = 32;

	if(argc > 1)
	{
		depth = ParsePositiveInt(argv[1]);
		if(depth < 0)
		{
			depth = 0;
		}
	}

	printf("stackpt: recursion depth=%d\n", depth);
	DumpUserPageTable("before");
	TouchStackRecursively(depth);
	DumpUserPageTable("after");
	printf("checksum=%x\n", g_stackChecksum);
	return 0;
}
