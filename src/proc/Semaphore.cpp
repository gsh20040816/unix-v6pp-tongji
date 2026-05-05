#include "Semaphore.h"
#include "Assembly.h"
#include "Kernel.h"
#include "Process.h"
#include "ProcessManager.h"
#include "User.h"

SemaphoreManager::SemaphoreManager()
{
}

SemaphoreManager::~SemaphoreManager()
{
}

void SemaphoreManager::Initialize()
{
	/* 所有槽位先置为空闲态，generation 从 1 开始，0 保留为非法 semId。 */
	for ( int i = 0; i < SemaphoreManager::NSEMA; ++i )
	{
		this->ResetEntry(this->m_Semaphores[i], 1);
	}
}

int SemaphoreManager::Init(int value)
{
	User& u = Kernel::Instance().GetUser();

	if ( value < 0 )
	{
		u.u_error = User::EINVAL;
		return 0;
	}

	X86Assembly::CLI();

	/* 槽位分配和 generation 读取必须在关中断下完成，避免并发复用。 */
	int slot = this->FindFreeSlot();
	if ( slot < 0 )
	{
		X86Assembly::STI();
		u.u_error = User::ENOSPC;
		return 0;
	}

	Semaphore& semaphore = this->m_Semaphores[slot];
	semaphore.used = true;
	semaphore.value = value;
	List::Init(&semaphore.waitQueue);

	/* 用户态只持有编码后的整数句柄，不直接暴露内核地址。 */
	u.u_ar0[User::EAX] = this->EncodeId(slot, semaphore.generation);

	X86Assembly::STI();
	return 0;
}

void SemaphoreManager::Wait(int semId)
{
	User& u = Kernel::Instance().GetUser();
	Process* current = u.u_procp;

	while ( true )
	{
		X86Assembly::CLI();

		/*
		 * 与 Sleep() 一样，进入可中断等待前先检查信号。
		 * 这样可以避免已经有待处理信号时仍然把进程挂到信号量队列上。
		 */
		if ( current->IsSig() )
		{
			X86Assembly::STI();
			aRetU(u.u_qsav);
			return;
		}

		Semaphore* semaphore = this->Lookup(semId);
		if ( semaphore == NULL )
		{
			X86Assembly::STI();
			u.u_error = User::EINVAL;
			return;
		}

		/*
		 * 只有在没有排队等待者时才直接消耗 value。
		 * 这样可以维持 FIFO 语义，避免新来的进程越过已经睡眠的等待者。
		 */
		if ( semaphore->value > 0 && List::Empty(&semaphore->waitQueue) )
		{
			semaphore->value--;
			current->p_semWakeReason = SemaphoreManager::WAKE_NONE;
			X86Assembly::STI();
			return;
		}

		/*
		 * Sleep() 可能因为信号被唤醒，也可能因为 Destroy()/Post() 被唤醒。
		 * 先把等待上下文挂到 Process 上，醒来后才能区分返回原因。
		 */
		current->p_semWaitSem = semaphore;
		current->p_semWakeReason = SemaphoreManager::WAKE_NONE;
		List::Init(&current->p_semNode);
		List::AddTail(&current->p_semNode, &semaphore->waitQueue);

		current->Sleep((unsigned long)semaphore, ProcessManager::PPIPE);

		if ( current->p_semWakeReason == SemaphoreManager::WAKE_GRANTED )
		{
			current->p_semWakeReason = SemaphoreManager::WAKE_NONE;
			return;
		}

		if ( current->p_semWakeReason == SemaphoreManager::WAKE_DESTROYED )
		{
			current->p_semWakeReason = SemaphoreManager::WAKE_NONE;
			u.u_error = User::EINVAL;
			return;
		}
	}
}

void SemaphoreManager::Post(int semId)
{
	User& u = Kernel::Instance().GetUser();

	X86Assembly::CLI();

	Semaphore* semaphore = this->Lookup(semId);
	if ( semaphore == NULL )
	{
		X86Assembly::STI();
		u.u_error = User::EINVAL;
		return;
	}

	if ( List::Empty(&semaphore->waitQueue) == false )
	{
		/* 直接转交给队头等待者，不增加 value，避免出现“已发放令牌又仍可重复领取”。 */
		Process* process =
			LIST_FIRST_ENTRY(&semaphore->waitQueue, Process, p_semNode);
		this->WakeProcess(*process, SemaphoreManager::WAKE_GRANTED);
	}
	else
	{
		/* 保持实现简单：溢出视为非法使用，而不是悄悄回绕。 */
		if ( semaphore->value == 0x7fffffff )
		{
			X86Assembly::STI();
			u.u_error = User::EINVAL;
			return;
		}
		semaphore->value++;
	}

	X86Assembly::STI();
}

void SemaphoreManager::Destroy(int semId)
{
	User& u = Kernel::Instance().GetUser();

	X86Assembly::CLI();

	Semaphore* semaphore = this->Lookup(semId);
	if ( semaphore == NULL )
	{
		X86Assembly::STI();
		u.u_error = User::EINVAL;
		return;
	}

	/* 先清空等待队列，再回收槽位，保证被唤醒进程只会看到 destroy 结果。 */
	while ( List::Empty(&semaphore->waitQueue) == false )
	{
		Process* process =
			LIST_FIRST_ENTRY(&semaphore->waitQueue, Process, p_semNode);
		this->WakeProcess(*process, SemaphoreManager::WAKE_DESTROYED);
	}

	/* generation 递增后，旧 semId 即使槽位复用也不会再次匹配。 */
	unsigned int nextGeneration = semaphore->generation + 1;
	if ( nextGeneration == 0 )
	{
		nextGeneration = 1;
	}
	this->ResetEntry(*semaphore, nextGeneration);

	X86Assembly::STI();
}

void SemaphoreManager::InterruptWait(Process& process)
{
	if ( process.p_semWaitSem == NULL )
	{
		return;
	}

	/*
	 * 信号打断只负责把进程从等待队列里摘掉。
	 * 最终返回到用户态的错误/信号语义仍由 Sleep()/Trap 路径统一处理。
	 */
	List::DeleteInit(&process.p_semNode);
	process.p_semWaitSem = NULL;
	process.p_semWakeReason = SemaphoreManager::WAKE_NONE;
}

Semaphore* SemaphoreManager::Lookup(int semId)
{
	int slot = 0;
	unsigned int generation = 0;

	if ( this->DecodeId(semId, slot, generation) == false )
	{
		return NULL;
	}

	Semaphore* semaphore = &this->m_Semaphores[slot];
	if ( semaphore->used == false || semaphore->generation != generation )
	{
		return NULL;
	}

	return semaphore;
}

int SemaphoreManager::FindFreeSlot() const
{
	for ( int i = 0; i < SemaphoreManager::NSEMA; ++i )
	{
		if ( this->m_Semaphores[i].used == false )
		{
			return i;
		}
	}
	return -1;
}

int SemaphoreManager::EncodeId(int slot, unsigned int generation) const
{
	/* 低 8 位存槽位，高位存代际号；当前 NSEMA=64，8 位槽位足够。 */
	return (int)((generation << 8) | (unsigned int)slot);
}

bool SemaphoreManager::DecodeId(int semId, int& slot, unsigned int& generation) const
{
	if ( semId < 0 )
	{
		return false;
	}

	slot = semId & 0xff;
	generation = ((unsigned int)semId) >> 8;

	if ( slot >= SemaphoreManager::NSEMA || generation == 0 )
	{
		return false;
	}
	return true;
}

void SemaphoreManager::ResetEntry(Semaphore& semaphore, unsigned int generation)
{
	semaphore.used = false;
	semaphore.value = 0;
	semaphore.generation = generation == 0 ? 1 : generation;
	List::Init(&semaphore.waitQueue);
}

void SemaphoreManager::WakeProcess(Process& process, int wakeReason)
{
	/* 唤醒前必须先脱链，避免同一进程被重复唤醒或残留在旧队列里。 */
	List::DeleteInit(&process.p_semNode);
	process.p_semWaitSem = NULL;
	process.p_semWakeReason = wakeReason;
	process.SetRun();
}
