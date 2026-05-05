#include <stdio.h>
#include <sys.h>

int main1(int argc, char* argv[])
{
	int semid = sem_init(0);
	if ( semid < 0 )
	{
		printf("sem_init failed\n");
		return 1;
	}

	int pid = fork();
	if ( pid < 0 )
	{
		printf("fork failed\n");
		sem_destroy(semid);
		return 1;
	}

	if ( pid == 0 )
	{
		printf("child waiting on %d\n", semid);
		if ( sem_wait(semid) < 0 )
		{
			printf("child sem_wait failed\n");
			exit(1);
		}
		printf("child acquired semaphore\n");
		exit(0);
	}

	sleep(2);
	printf("parent posting semaphore\n");
	if ( sem_post(semid) < 0 )
	{
		printf("parent sem_post failed\n");
		sem_destroy(semid);
		return 1;
	}

	sleep(2);
	if ( sem_destroy(semid) < 0 )
	{
		printf("sem_destroy failed\n");
		return 1;
	}

	printf("semtest done\n");
	return 0;
}
