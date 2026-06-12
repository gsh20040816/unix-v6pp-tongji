#include <stdio.h>
#include <sys.h>

#define PROCESS_COUNT 4
#define PEER_COUNT (PROCESS_COUNT - 1)

static void DestroySemaphores(int sems[], int count)
{
	int i;

	for(i = 0; i < count; i++)
	{
		if(sems[i] >= 0)
		{
			sem_destroy(sems[i]);
		}
	}
}

static int NotifyArrival(int role, int sems[])
{
	int i;

	for(i = 0; i < PEER_COUNT; i++)
	{
		if(sem_post(sems[role]) < 0)
		{
			printf("P%d V(S%d) failed\n", role + 1, role + 1);
			return -1;
		}
	}

	return 0;
}

static int WaitForPeers(int role, int sems[])
{
	int i;

	for(i = 0; i < PROCESS_COUNT; i++)
	{
		if(i == role)
		{
			continue;
		}

		if(sem_wait(sems[i]) < 0)
		{
			printf("P%d P(S%d) failed\n", role + 1, i + 1);
			return -1;
		}
	}

	return 0;
}

static int RunParticipant(int role, int sems[])
{
	printf("P%d_1 done\n", role + 1);

	if(NotifyArrival(role, sems) < 0)
	{
		return 1;
	}

	if(WaitForPeers(role, sems) < 0)
	{
		return 1;
	}

	printf("P%d_2 start\n", role + 1);
	return 0;
}

static void ReapChildren(int childCount, int* status)
{
	int i;
	int child;
	int childStatus;

	for(i = 0; i < childCount; i++)
	{
		childStatus = 0;
		child = wait(&childStatus);
		if(child < 0)
		{
			printf("wait failed\n");
			*status = 1;
			continue;
		}

		if(childStatus != 0)
		{
			printf("child %d exited with %d\n", child, childStatus);
			*status = 1;
		}
	}
}

int main1(int argc, char* argv[])
{
	int sems[PROCESS_COUNT];
	int i;
	int pid;
	int childCount;
	int status;
	int destroyed;

	for(i = 0; i < PROCESS_COUNT; i++)
	{
		sems[i] = -1;
	}

	for(i = 0; i < PROCESS_COUNT; i++)
	{
		sems[i] = sem_init(0);
		if(sems[i] < 0)
		{
			printf("sem_init S%d failed\n", i + 1);
			DestroySemaphores(sems, PROCESS_COUNT);
			return 1;
		}
	}

	childCount = 0;
	status = 0;
	destroyed = 0;
	for(i = 1; i < PROCESS_COUNT; i++)
	{
		pid = fork();
		if(pid < 0)
		{
			printf("fork P%d failed\n", i + 1);
			DestroySemaphores(sems, PROCESS_COUNT);
			ReapChildren(childCount, &status);
			return 1;
		}

		if(pid == 0)
		{
			status = RunParticipant(i, sems);
			exit(status);
		}

		childCount++;
	}

	status = RunParticipant(0, sems);
	if(status != 0)
	{
		DestroySemaphores(sems, PROCESS_COUNT);
		destroyed = 1;
	}

	ReapChildren(childCount, &status);

	if(destroyed == 0)
	{
		DestroySemaphores(sems, PROCESS_COUNT);
	}

	if(status == 0)
	{
		printf("sembarrier done\n");
	}
	else
	{
		printf("sembarrier failed\n");
	}

	return status;
}
