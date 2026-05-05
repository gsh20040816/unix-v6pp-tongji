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

		if ( semaphore->value > 0 && List::Empty(&semaphore->waitQueue) )
		{
			semaphore->value--;
			current->p_semWakeReason = SemaphoreManager::WAKE_NONE;
			X86Assembly::STI();
			return;
		}

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
		Process* process =
			LIST_FIRST_ENTRY(&semaphore->waitQueue, Process, p_semNode);
		this->WakeProcess(*process, SemaphoreManager::WAKE_GRANTED);
	}
	else
	{
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

	while ( List::Empty(&semaphore->waitQueue) == false )
	{
		Process* process =
			LIST_FIRST_ENTRY(&semaphore->waitQueue, Process, p_semNode);
		this->WakeProcess(*process, SemaphoreManager::WAKE_DESTROYED);
	}

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
	List::DeleteInit(&process.p_semNode);
	process.p_semWaitSem = NULL;
	process.p_semWakeReason = wakeReason;
	process.SetRun();
}
