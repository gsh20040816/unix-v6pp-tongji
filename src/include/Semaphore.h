#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "List.h"

class Process;

struct Semaphore
{
	bool used;
	int value;
	unsigned int generation;
	ListHead waitQueue;
};

class SemaphoreManager
{
public:
	static const int NSEMA = 64;

	enum WaitWakeReason
	{
		WAKE_NONE = 0,
		WAKE_GRANTED = 1,
		WAKE_DESTROYED = 2
	};

public:
	SemaphoreManager();
	~SemaphoreManager();

	void Initialize();

	int Init(int value);
	void Wait(int semId);
	void Post(int semId);
	void Destroy(int semId);

	void InterruptWait(Process& process);

private:
	Semaphore* Lookup(int semId);
	int FindFreeSlot() const;
	int EncodeId(int slot, unsigned int generation) const;
	bool DecodeId(int semId, int& slot, unsigned int& generation) const;
	void ResetEntry(Semaphore& semaphore, unsigned int generation);
	void WakeProcess(Process& process, int wakeReason);

private:
	Semaphore m_Semaphores[NSEMA];
};

#endif
