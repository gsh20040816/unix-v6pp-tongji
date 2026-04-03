#include "PageManager.h"
#include "Allocator.h"

unsigned int PageManager::PHY_MEM_SIZE;
unsigned int UserPageManager::USER_PAGE_POOL_SIZE;

PageManager::PageManager(Allocator* allocator)
{
	this->m_pAllocator = allocator;
}

int PageManager::Initialize()
{
	for ( unsigned int i = 0; i < MEMORY_MAP_ARRAY_SIZE; i++ ) 
	{
		this->map[i].m_AddressIdx = 0;
		this->map[i].m_Size = 0;
	}
	return 0;
}

unsigned long PageManager::AllocMemory(unsigned long size)
{
	if ( size == 0 )
	{
		return 0;
	}

	unsigned long pageCount = (size + (PAGE_SIZE - 1)) / PAGE_SIZE;
	return this->m_pAllocator->AllocCheckedOneUnit(this->map, pageCount) * PAGE_SIZE;
}

unsigned long PageManager::FreeMemory(unsigned long size, unsigned long startAddress)
{
	if ( size == 0 || startAddress == 0 )
	{
		return 0;
	}

	unsigned long pageCount = (size + (PAGE_SIZE - 1)) / PAGE_SIZE;
	return this->m_pAllocator->FreeCheckedOneUnit(this->map, pageCount, startAddress / PAGE_SIZE);
}

PageManager::~PageManager()
{
}

KernelPageManager::KernelPageManager(Allocator* allocator)
	:PageManager(allocator)
{
}

int KernelPageManager::Initialize()
{
	PageManager::Initialize();
	
	this->map[0].m_AddressIdx = 
		KERNEL_PAGE_POOL_START_ADDR / PageManager::PAGE_SIZE;
	this->map[0].m_Size = 
		KERNEL_PAGE_POOL_SIZE / PageManager::PAGE_SIZE;
	return 0;
}

UserPageManager::UserPageManager(Allocator* allocator)
	:PageManager(allocator)
{
}

int UserPageManager::Initialize()
{
	PageManager::Initialize();
	
	this->map[0].m_AddressIdx = 
		USER_PAGE_POOL_START_ADDR / PageManager::PAGE_SIZE;
	this->map[0].m_Size = 
		USER_PAGE_POOL_SIZE / PageManager::PAGE_SIZE;
	return 0;
}

