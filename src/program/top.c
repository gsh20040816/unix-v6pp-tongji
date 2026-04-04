#include <stdio.h>
#include <sys.h>
#include <malloc.h>

#define DEFAULT_ROUNDS 8
#define DEFAULT_ALLOC_KB 128
#define TOUCH_STRIDE 64

static int ParsePositiveInt(const char* text, int defaultValue)
{
	int value = 0;
	int i = 0;

	if ( text == 0 || text[0] == '\0' )
	{
		return defaultValue;
	}

	if ( text[0] == '+' )
	{
		i = 1;
	}

	for ( ; text[i] != '\0'; ++i )
	{
		if ( text[i] < '0' || text[i] > '9' )
		{
			return defaultValue;
		}
		value = value * 10 + (text[i] - '0');
	}

	if ( value <= 0 )
	{
		return defaultValue;
	}

	return value;
}

static void TouchBuffer(char* buffer, unsigned int size, unsigned char seed)
{
	unsigned int i;

	if ( buffer == 0 || size == 0 )
	{
		return;
	}

	for ( i = 0; i < size; i += TOUCH_STRIDE )
	{
		buffer[i] = (char)(seed + (unsigned char)i);
	}

	buffer[size - 1] = (char)(seed ^ 0x5A);
}

int main1(int argc, char* argv[])
{
	int rounds = DEFAULT_ROUNDS;
	int allocKB = DEFAULT_ALLOC_KB;
	int baseline;
	int leakRounds = 0;
	int i;

	if ( argc > 1 )
	{
		rounds = ParsePositiveInt(argv[1], DEFAULT_ROUNDS);
	}
	if ( argc > 2 )
	{
		allocKB = ParsePositiveInt(argv[2], DEFAULT_ALLOC_KB);
	}

	if ( rounds <= 0 )
	{
		rounds = DEFAULT_ROUNDS;
	}
	if ( allocKB <= 0 )
	{
		allocKB = DEFAULT_ALLOC_KB;
	}

	baseline = getUserFreeMemory();
	if ( baseline < 0 )
	{
		printf("top: getUserFreeMemory failed\n");
		return 1;
	}

	printf("top: rounds=%d alloc=%dKB\n", rounds, allocKB);
	printf("top: baseline free=%d bytes (%d KB)\n", baseline, baseline / 1024);

	for ( i = 0; i < rounds; ++i )
	{
		int before = getUserFreeMemory();
		int status = 0;
		int pid;

		if ( before < 0 )
		{
			printf("round %d: query before failed\n", i + 1);
			return 1;
		}

		pid = fork();
		if ( pid < 0 )
		{
			printf("round %d: fork failed\n", i + 1);
			return 1;
		}
		else if ( pid == 0 )
		{
			unsigned int allocBytes = (unsigned int)allocKB * 1024;
			char* p = (char*)malloc(allocBytes);
			if ( p != 0 )
			{
				TouchBuffer(p, allocBytes, (unsigned char)(i + 1));
			}
			exit(0);
		}
		else
		{
			int after;

			if ( wait(&status) < 0 )
			{
				printf("round %d: wait failed\n", i + 1);
				return 1;
			}

			after = getUserFreeMemory();
			if ( after < 0 )
			{
				printf("round %d: query after failed\n", i + 1);
				return 1;
			}

			printf("round %d: before=%d after=%d delta=%d\n",
				i + 1, before, after, after - before);

			if ( after != before )
			{
				leakRounds++;
			}
		}
	}

	{
		int finalFree = getUserFreeMemory();
		if ( finalFree >= 0 )
		{
			printf("top: final free=%d bytes (%d KB), baseline delta=%d\n",
				finalFree, finalFree / 1024, finalFree - baseline);
		}
	}

	if ( leakRounds == 0 )
	{
		printf("top: no leak detected in %d rounds\n", rounds);
	}
	else
	{
		printf("top: potential leak detected in %d/%d rounds\n", leakRounds, rounds);
	}

	return 0;
}
