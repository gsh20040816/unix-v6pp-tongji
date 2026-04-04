#include "PageManager.h"
#include "Utility.h"

unsigned int PageManager::PHY_MEM_SIZE;
unsigned int UserPageManager::USER_PAGE_POOL_SIZE;

PageManager::PageManager()
{
	this->m_PoolStartAddress = 0;
	this->m_TotalPageCount = 0;
}

int PageManager::Initialize()
{
	for ( unsigned int i = 0; i < BITMAP_WORD_COUNT; ++i )
	{
		this->m_Bitmap[i] = 0;
	}
	this->m_PoolStartAddress = 0;
	this->m_TotalPageCount = 0;
	return 0;
}

int PageManager::InitializePool(unsigned long poolStartAddress, unsigned long poolSizeBytes)
{
	this->Initialize();
	this->m_PoolStartAddress = poolStartAddress;
	this->m_TotalPageCount = poolSizeBytes / PAGE_SIZE;

	if ( this->m_TotalPageCount > MAX_BITMAP_PAGE_COUNT )
	{
		Utility::Panic("Page bitmap capacity exceeded");
	}
	return 0;
}

bool PageManager::IsPageUsed(unsigned long pageIndex) const
{
	unsigned long wordIndex = pageIndex / BITMAP_WORD_BITS;
	unsigned long bitOffset = pageIndex % BITMAP_WORD_BITS;
	return (this->m_Bitmap[wordIndex] & (1UL << bitOffset)) != 0;
}

void PageManager::MarkPagesUsed(unsigned long startPage, unsigned long pageCount)
{
	for ( unsigned long i = 0; i < pageCount; ++i )
	{
		unsigned long page = startPage + i;
		unsigned long wordIndex = page / BITMAP_WORD_BITS;
		unsigned long bitOffset = page % BITMAP_WORD_BITS;
		this->m_Bitmap[wordIndex] |= (1UL << bitOffset);
	}
}

void PageManager::MarkPagesFree(unsigned long startPage, unsigned long pageCount)
{
	for ( unsigned long i = 0; i < pageCount; ++i )
	{
		unsigned long page = startPage + i;
		unsigned long wordIndex = page / BITMAP_WORD_BITS;
		unsigned long bitOffset = page % BITMAP_WORD_BITS;
		this->m_Bitmap[wordIndex] &= ~(1UL << bitOffset);
	}
}

unsigned long PageManager::AllocatePages(unsigned long pageCount)
{
	if ( pageCount == 0 || this->m_TotalPageCount == 0 || pageCount > this->m_TotalPageCount )
	{
		return 0;
	}

	unsigned long runStart = 0;
	unsigned long runLength = 0;
	for ( unsigned long page = 0; page < this->m_TotalPageCount; ++page )
	{
		if ( this->IsPageUsed(page) )
		{
			runLength = 0;
			continue;
		}

		if ( runLength == 0 )
		{
			runStart = page;
		}
		++runLength;

		if ( runLength == pageCount )
		{
			this->MarkPagesUsed(runStart, pageCount);
			return this->m_PoolStartAddress + runStart * PAGE_SIZE;
		}
	}

	return 0;
}

unsigned long PageManager::FreePages(unsigned long pageCount, unsigned long startAddress)
{
	if ( pageCount == 0 || startAddress < this->m_PoolStartAddress )
	{
		return 0;
	}

	unsigned long offset = startAddress - this->m_PoolStartAddress;
	if ( offset % PAGE_SIZE != 0 )
	{
		return 0;
	}

	unsigned long startPage = offset / PAGE_SIZE;
	if ( pageCount > this->m_TotalPageCount || startPage > this->m_TotalPageCount - pageCount )
	{
		return 0;
	}

	this->MarkPagesFree(startPage, pageCount);
	return 0;
}

unsigned long PageManager::AllocatePage()
{
	return this->AllocatePages(1);
}

unsigned long PageManager::FreePage(unsigned long startAddress)
{
	return this->FreePages(1, startAddress);
}

unsigned long PageManager::GetFreePageCount() const
{
	unsigned long freeCount = 0;
	for ( unsigned long page = 0; page < this->m_TotalPageCount; ++page )
	{
		if ( this->IsPageUsed(page) == false )
		{
			++freeCount;
		}
	}
	return freeCount;
}

unsigned long PageManager::GetTotalPageCount() const
{
	return this->m_TotalPageCount;
}

PageManager::~PageManager()
{
}

KernelPageManager::KernelPageManager()
	:PageManager()
{
}

int KernelPageManager::Initialize()
{
	return this->InitializePool(
		KERNEL_PAGE_POOL_START_ADDR,
		KERNEL_PAGE_POOL_SIZE);
}

UserPageManager::UserPageManager()
	:PageManager()
{
}

int UserPageManager::Initialize()
{
	return this->InitializePool(
		USER_PAGE_POOL_START_ADDR,
		USER_PAGE_POOL_SIZE);
}

