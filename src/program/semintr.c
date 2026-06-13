#include <stdio.h>
#include <sys.h>

static int p2Interrupted = 0;

static void P2SignalHandler()
{
	p2Interrupted = 1;
}

static int RunP2(int semid, int readySem)
{
	int result;

	if(signal(SIGUSR1, P2SignalHandler) < 0)
	{
		printf("P2: signal install failed\n");
		return 1;
	}

	printf("P2: waiting on semaphore\n");
	if(sem_post(readySem) < 0)
	{
		printf("P2: ready notify failed\n");
		return 1;
	}

	result = sem_wait(semid);
	if(p2Interrupted != 0)
	{
		printf("P2: sem_wait interrupted\n");
		return 0;
	}
	if(result < 0)
	{
		printf("P2: sem_wait failed\n");
		return 1;
	}

	printf("P2: acquired semaphore\n");
	return 1;
}

static int RunP3(int semid, int p2pid)
{
	int result;

	sleep(1);
	printf("P3: interrupting P2\n");
	if(kill(p2pid, SIGUSR1) < 0)
	{
		printf("P3: kill P2 failed\n");
		return 1;
	}

	printf("P3: waiting on semaphore\n");
	result = sem_wait(semid);
	if(result < 0)
	{
		printf("P3: sem_wait failed\n");
		return 1;
	}

	printf("P3: acquired semaphore\n");
	return 0;
}

int main1(int argc, char* argv[])
{
	int semid;
	int readySem;
	int p2pid;
	int p3pid;
	int status;
	int failed;
	int waited;
	int child;

	(void)argc;
	(void)argv;

	semid = sem_init(0);
	if(semid < 0)
	{
		printf("P1: sem_init failed\n");
		return 1;
	}

	readySem = sem_init(0);
	if(readySem < 0)
	{
		printf("P1: ready sem_init failed\n");
		sem_destroy(semid);
		return 1;
	}

	p2pid = fork();
	if(p2pid < 0)
	{
		printf("P1: fork P2 failed\n");
		sem_destroy(readySem);
		sem_destroy(semid);
		return 1;
	}

	if(p2pid == 0)
	{
		exit(RunP2(semid, readySem));
	}

	if(sem_wait(readySem) < 0)
	{
		printf("P1: wait P2 ready failed\n");
		kill(p2pid, SIGKILL);
		sem_destroy(readySem);
		sem_destroy(semid);
		return 1;
	}

	p3pid = fork();
	if(p3pid < 0)
	{
		printf("P1: fork P3 failed\n");
		kill(p2pid, SIGKILL);
		sem_destroy(readySem);
		sem_destroy(semid);
		return 1;
	}

	if(p3pid == 0)
	{
		exit(RunP3(semid, p2pid));
	}

	sleep(3);
	printf("P1: posting semaphore\n");
	if(sem_post(semid) < 0)
	{
		printf("P1: sem_post failed\n");
		kill(p2pid, SIGKILL);
		kill(p3pid, SIGKILL);
		sem_destroy(readySem);
		sem_destroy(semid);
		return 1;
	}

	failed = 0;
	for(waited = 0; waited < 2; waited++)
	{
		status = 0;
		child = wait(&status);
		if(child < 0)
		{
			printf("P1: wait failed\n");
			failed = 1;
			continue;
		}

		if(status != 0)
		{
			printf("P1: child %d exited with %d\n", child, status);
			failed = 1;
		}
	}

	if(sem_destroy(semid) < 0)
	{
		printf("P1: sem_destroy failed\n");
		failed = 1;
	}
	if(sem_destroy(readySem) < 0)
	{
		printf("P1: ready sem_destroy failed\n");
		failed = 1;
	}

	if(failed == 0)
	{
		printf("semintr done\n");
	}
	else
	{
		printf("semintr failed\n");
	}

	return failed;
}
