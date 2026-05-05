#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "List.h"

class Process;

struct Semaphore
{
	bool used;
	int value;
	/* 代际号与槽位一起编码成 semId，避免销毁后旧句柄误指向新对象。 */
	unsigned int generation;
	/* 等待队列中的节点属于 Process，本结构只维护队头/队尾。 */
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

	/* 分配一个可复用槽位，并把返回值写到用户态 EAX。 */
	int Init(int value);
	/* P 操作可能因为真正获取到资源、对象被销毁或信号打断而返回。 */
	void Wait(int semId);
	/* 优先把资源直接转交给等待者，避免 value 和等待队列同时非空。 */
	void Post(int semId);
	/* 销毁后必须唤醒所有等待者，并让旧 semId 立即失效。 */
	void Destroy(int semId);

	/* 信号打断睡眠时，把进程从信号量等待队列中摘掉。 */
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
