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
	for ( unsigned int i = 0; i < PageManager::MAX_BITMAP_PAGE_COUNT; ++i )
	{
		this->m_CowRefCount[i] = 0;
	}
}

int UserPageManager::Initialize()
{
	int ret = this->InitializePool(
		USER_PAGE_POOL_START_ADDR,
		USER_PAGE_POOL_SIZE);
	for ( unsigned int i = 0; i < PageManager::MAX_BITMAP_PAGE_COUNT; ++i )
	{
		this->m_CowRefCount[i] = 0;
	}

	/*
	 * 固定保留用户区第一页(0x400000)作为全局零页：
	 * 1) 不参与普通分配；
	 * 2) BACKING_ZERO 缺页时统一只读映射到该页；
	 * 3) 首次写入通过 COW 分裂获得私有页。
	 */
	unsigned long zeroPage = PageManager::AllocatePage();
	if ( zeroPage != USER_ZERO_PAGE_ADDRESS )
	{
		Utility::Panic("User zero page init failed");
	}
	this->m_CowRefCount[0] = 1;	/* 基线引用，防止零页被当作普通 COW 页释放。 */
	Utility::ZeroPage(zeroPage);

	return ret;
}

bool UserPageManager::ResolvePoolPageIndex(unsigned long pageAddress,
	unsigned long& pageIndex) const
{
	if ( pageAddress < USER_PAGE_POOL_START_ADDR )
	{
		return false;
	}

	unsigned long offset = pageAddress - USER_PAGE_POOL_START_ADDR;
	if ( offset % PageManager::PAGE_SIZE != 0 )
	{
		return false;
	}

	pageIndex = offset / PageManager::PAGE_SIZE;
	if ( pageIndex >= this->GetTotalPageCount() )
	{
		return false;
	}

	return true;
}

unsigned long UserPageManager::AllocatePages(unsigned long pageCount)
{
	unsigned long startAddress = PageManager::AllocatePages(pageCount);
	if ( startAddress == 0 )
	{
		return 0;
	}

	unsigned long startIndex = 0;
	if ( this->ResolvePoolPageIndex(startAddress, startIndex) )
	{
		for ( unsigned long i = 0; i < pageCount; ++i )
		{
			this->m_CowRefCount[startIndex + i] = 0;
		}
	}

	return startAddress;
}

unsigned long UserPageManager::FreePages(unsigned long pageCount,
	unsigned long startAddress)
{
	if ( pageCount == 0 )
	{
		return 0;
	}

	for ( unsigned long i = 0; i < pageCount; ++i )
	{
		unsigned long pageAddress =
			startAddress + i * PageManager::PAGE_SIZE;

		if ( this->IsZeroPage(pageAddress) )
		{
			continue;
		}

		unsigned long pageIndex = 0;
		if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) )
		{
			this->m_CowRefCount[pageIndex] = 0;
		}

		PageManager::FreePage(pageAddress);
	}

	return 0;
}

unsigned long UserPageManager::AllocatePage()
{
	return this->AllocatePages(1);
}

unsigned long UserPageManager::FreePage(unsigned long startAddress)
{
	return this->FreePages(1, startAddress);
}

unsigned long UserPageManager::GetZeroPageAddress() const
{
	return USER_ZERO_PAGE_ADDRESS;
}

bool UserPageManager::IsZeroPage(unsigned long pageAddress) const
{
	return pageAddress == USER_ZERO_PAGE_ADDRESS;
}

bool UserPageManager::ShareAsCopyOnWrite(unsigned long pageAddress)
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return false;
	}

	if ( this->m_CowRefCount[pageIndex] == 0 )
	{
		this->m_CowRefCount[pageIndex] = 2;
		return true;
	}

	if ( this->m_CowRefCount[pageIndex] == 0xffff )
	{
		return false;
	}

	this->m_CowRefCount[pageIndex]++;
	return true;
}

unsigned short UserPageManager::GetCopyOnWriteRefCount(unsigned long pageAddress) const
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return 0;
	}

	return this->m_CowRefCount[pageIndex];
}

unsigned short UserPageManager::ReleaseCopyOnWriteRef(unsigned long pageAddress)
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return 0;
	}

	if ( this->m_CowRefCount[pageIndex] == 0 )
	{
		return 0;
	}

	this->m_CowRefCount[pageIndex]--;
	return this->m_CowRefCount[pageIndex];
}

void UserPageManager::ClearCopyOnWriteRef(unsigned long pageAddress)
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return;
	}

	this->m_CowRefCount[pageIndex] = 0;
}

