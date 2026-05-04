#include "PageManager.h"
#include "Utility.h"
#include "Kernel.h"
#include "MemoryDescriptor.h"
#include "SwapManager.h"
#include "Assembly.h"

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
		List::Init(&this->m_FrameInfo[i].rmapHead);
		this->m_FrameInfo[i].mapCount = 0;
		this->m_FrameInfo[i].flags = FRAME_FLAG_NONE;
		this->m_FrameInfo[i].cowRefCount = 0;
		this->m_FrameInfo[i].clockAge = 0;
	}
	this->m_ClockHand = 0;
	this->m_Reclaiming = false;
}

int UserPageManager::Initialize()
{
	int ret = this->InitializePool(
		USER_PAGE_POOL_START_ADDR,
		USER_PAGE_POOL_SIZE);
	for ( unsigned int i = 0; i < PageManager::MAX_BITMAP_PAGE_COUNT; ++i )
	{
		List::Init(&this->m_FrameInfo[i].rmapHead);
		this->m_FrameInfo[i].mapCount = 0;
		this->m_FrameInfo[i].flags = FRAME_FLAG_NONE;
		this->m_FrameInfo[i].cowRefCount = 0;
		this->m_FrameInfo[i].clockAge = 0;
	}
	this->m_ClockHand = 0;
	this->m_Reclaiming = false;

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
	this->m_FrameInfo[0].cowRefCount = 1;	/* 基线引用，防止零页被当作普通 COW 页释放。 */
	this->m_FrameInfo[0].flags = FRAME_FLAG_ZERO_PAGE | FRAME_FLAG_COW | FRAME_FLAG_PINNED;
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
	if ( this->m_Reclaiming == false && this->ShouldReclaimBeforeAllocate(pageCount) )
	{
		this->ReclaimUntilLowWatermark();
	}

	unsigned long startAddress = PageManager::AllocatePages(pageCount);
	if ( startAddress == 0 && this->m_Reclaiming == false )
	{
		this->ReclaimUntilLowWatermark();
		startAddress = PageManager::AllocatePages(pageCount);
	}
	if ( startAddress == 0 )
	{
		return 0;
	}

	unsigned long startIndex = 0;
	if ( this->ResolvePoolPageIndex(startAddress, startIndex) )
	{
		for ( unsigned long i = 0; i < pageCount; ++i )
		{
			this->m_FrameInfo[startIndex + i].mapCount = 0;
			this->m_FrameInfo[startIndex + i].flags = FRAME_FLAG_NONE;
			this->m_FrameInfo[startIndex + i].cowRefCount = 0;
			this->m_FrameInfo[startIndex + i].clockAge = 0;
			List::Init(&this->m_FrameInfo[startIndex + i].rmapHead);
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
			if ( this->m_FrameInfo[pageIndex].mapCount != 0 )
			{
				Utility::Panic("Free mapped user page");
			}
			this->m_FrameInfo[pageIndex].cowRefCount = 0;
			this->m_FrameInfo[pageIndex].flags = FRAME_FLAG_NONE;
			this->m_FrameInfo[pageIndex].clockAge = 0;
			List::Init(&this->m_FrameInfo[pageIndex].rmapHead);
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

UserPageManager::FrameInfo* UserPageManager::GetFrameInfoByAddress(unsigned long pageAddress)
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return NULL;
	}
	return &this->m_FrameInfo[pageIndex];
}

const UserPageManager::FrameInfo* UserPageManager::GetFrameInfoByAddress(
	unsigned long pageAddress) const
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return NULL;
	}
	return &this->m_FrameInfo[pageIndex];
}

unsigned long UserPageManager::GetPageAddressByIndex(unsigned long pageIndex) const
{
	return USER_PAGE_POOL_START_ADDR + pageIndex * PageManager::PAGE_SIZE;
}

bool UserPageManager::AttachReverseMap(unsigned long pageAddress,
	ReverseMapEntry* entry,
	unsigned short frameFlags)
{
	FrameInfo* frame = this->GetFrameInfoByAddress(pageAddress);
	if ( frame == NULL || entry == NULL )
	{
		return false;
	}

	if ( entry->frameNode.next == NULL || entry->frameNode.prev == NULL )
	{
		List::Init(&entry->frameNode);
	}
	List::AddTail(&entry->frameNode, &frame->rmapHead);
	++frame->mapCount;
	frame->flags |= frameFlags;
	return true;
}

void UserPageManager::DetachReverseMap(unsigned long pageAddress,
	ReverseMapEntry* entry)
{
	FrameInfo* frame = this->GetFrameInfoByAddress(pageAddress);
	if ( frame == NULL || entry == NULL )
	{
		return;
	}

	if ( entry->frameNode.next != NULL && entry->frameNode.prev != NULL )
	{
		List::Delete(&entry->frameNode);
	}
	if ( frame->mapCount != 0 )
	{
		--frame->mapCount;
	}

	if ( frame->cowRefCount != 0 )
	{
		this->ReleaseCopyOnWriteRef(pageAddress);
	}

	if ( frame->mapCount == 0 )
	{
		frame->flags &= FRAME_FLAG_ZERO_PAGE | FRAME_FLAG_PINNED;
		if ( this->IsZeroPage(pageAddress) == false )
		{
			frame->cowRefCount = 0;
		}
	}
}

unsigned short UserPageManager::GetFrameMapCount(unsigned long pageAddress) const
{
	const FrameInfo* frame = this->GetFrameInfoByAddress(pageAddress);
	return frame == NULL ? 0 : frame->mapCount;
}

unsigned short UserPageManager::GetFrameFlags(unsigned long pageAddress) const
{
	const FrameInfo* frame = this->GetFrameInfoByAddress(pageAddress);
	return frame == NULL ? FRAME_FLAG_NONE : frame->flags;
}

void UserPageManager::SetFrameFlags(unsigned long pageAddress, unsigned short flags)
{
	FrameInfo* frame = this->GetFrameInfoByAddress(pageAddress);
	if ( frame != NULL )
	{
		frame->flags = flags;
	}
}

bool UserPageManager::ShareAsCopyOnWrite(unsigned long pageAddress)
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return false;
	}

	if ( this->m_FrameInfo[pageIndex].cowRefCount == 0 )
	{
		this->m_FrameInfo[pageIndex].cowRefCount = 2;
		this->m_FrameInfo[pageIndex].flags |= FRAME_FLAG_COW;
		return true;
	}

	if ( this->m_FrameInfo[pageIndex].cowRefCount == 0xffff )
	{
		return false;
	}

	this->m_FrameInfo[pageIndex].cowRefCount++;
	this->m_FrameInfo[pageIndex].flags |= FRAME_FLAG_COW;
	return true;
}

unsigned short UserPageManager::GetCopyOnWriteRefCount(unsigned long pageAddress) const
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return 0;
	}

	return this->m_FrameInfo[pageIndex].cowRefCount;
}

unsigned short UserPageManager::ReleaseCopyOnWriteRef(unsigned long pageAddress)
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return 0;
	}

	if ( this->m_FrameInfo[pageIndex].cowRefCount == 0 )
	{
		return 0;
	}

	this->m_FrameInfo[pageIndex].cowRefCount--;
	if ( this->m_FrameInfo[pageIndex].cowRefCount == 0 )
	{
		this->m_FrameInfo[pageIndex].flags &= ~FRAME_FLAG_COW;
	}
	return this->m_FrameInfo[pageIndex].cowRefCount;
}

bool UserPageManager::SetCopyOnWriteRefCount(unsigned long pageAddress,
	unsigned short refCount)
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return false;
	}

	this->m_FrameInfo[pageIndex].cowRefCount = refCount;
	if ( refCount == 0 )
	{
		this->m_FrameInfo[pageIndex].flags &= ~FRAME_FLAG_COW;
	}
	else
	{
		this->m_FrameInfo[pageIndex].flags |= FRAME_FLAG_COW;
	}
	return true;
}

void UserPageManager::ClearCopyOnWriteRef(unsigned long pageAddress)
{
	unsigned long pageIndex = 0;
	if ( this->ResolvePoolPageIndex(pageAddress, pageIndex) == false )
	{
		return;
	}

	this->m_FrameInfo[pageIndex].cowRefCount = 0;
	this->m_FrameInfo[pageIndex].flags &= ~FRAME_FLAG_COW;
}

bool UserPageManager::ShouldReclaimBeforeAllocate(unsigned long pageCount) const
{
	unsigned long total = this->GetTotalPageCount();
	if ( total == 0 )
	{
		return false;
	}
	unsigned long used = total - this->GetFreePageCount();
	return (used + pageCount) * 100 >= total * RECLAIM_HIGH_WATERMARK_PERCENT;
}

bool UserPageManager::ReclaimUntilLowWatermark()
{
	unsigned long total = this->GetTotalPageCount();
	if ( total == 0 || Kernel::Instance().GetSwapManager().IsInitialized() == false )
	{
		return false;
	}

	this->m_Reclaiming = true;
	unsigned int scannedRounds = 0;
	while ( (total - this->GetFreePageCount()) * 100 >
		total * RECLAIM_LOW_WATERMARK_PERCENT )
	{
		if ( this->ReclaimOneFrame() == false )
		{
			++scannedRounds;
			if ( scannedRounds > 2 )
			{
				break;
			}
		}
		else
		{
			scannedRounds = 0;
		}
	}
	this->m_Reclaiming = false;
	return true;
}

bool UserPageManager::ReclaimOneFrame()
{
	unsigned long total = this->GetTotalPageCount();
	if ( total == 0 )
	{
		return false;
	}

	for ( unsigned long scanned = 0; scanned < total; ++scanned )
	{
		unsigned long pageIndex = this->m_ClockHand;
		this->m_ClockHand = (this->m_ClockHand + 1) % total;
		FrameInfo& frame = this->m_FrameInfo[pageIndex];
		if ( frame.mapCount == 0 ||
			(frame.flags & (FRAME_FLAG_ZERO_PAGE | FRAME_FLAG_PINNED)) != 0 )
		{
			continue;
		}

		bool accessed = false;
		bool dirty = false;
		bool discardable = (frame.flags & FRAME_FLAG_COW) == 0;
		bool valid = true;
		ListHead* pos = NULL;
		LIST_FOR_EACH(pos, &frame.rmapHead)
		{
			ReverseMapEntry* entry = LIST_ENTRY(pos, ReverseMapEntry, frameNode);
			bool entryAccessed = false;
			bool entryDirty = false;
			bool entryDiscardable = false;
			if ( entry->owner == NULL ||
				entry->owner->CollectEvictionInfo(entry->virtualPageIndex,
					entryAccessed,
					entryDirty,
					entryDiscardable) == false )
			{
				valid = false;
				break;
			}
			accessed = accessed || entryAccessed;
			dirty = dirty || entryDirty;
			discardable = discardable && entryDiscardable;
		}
		if ( valid == false )
		{
			continue;
		}

		if ( accessed )
		{
			LIST_FOR_EACH(pos, &frame.rmapHead)
			{
				ReverseMapEntry* entry = LIST_ENTRY(pos, ReverseMapEntry, frameNode);
				entry->owner->ClearPageAccessed(entry->virtualPageIndex);
			}
			X86Assembly::FlushCurrentPageDirectory();
			continue;
		}

		unsigned long physicalAddress = this->GetPageAddressByIndex(pageIndex);
		if ( discardable && dirty == false )
		{
			while ( List::Empty(&frame.rmapHead) == false )
			{
				ReverseMapEntry* entry =
					LIST_FIRST_ENTRY(&frame.rmapHead, ReverseMapEntry, frameNode);
				if ( entry->owner->EvictPageToReserved(entry->virtualPageIndex) == false )
				{
					return false;
				}
			}
			X86Assembly::FlushCurrentPageDirectory();
			return true;
		}

		unsigned int slot =
			Kernel::Instance().GetSwapManager().AllocateSlot(frame.mapCount);
		if ( slot == SwapManager::INVALID_SWAP_SLOT )
		{
			return false;
		}

		frame.flags |= FRAME_FLAG_PINNED;
		if ( Kernel::Instance().GetSwapManager().WritePage(slot, physicalAddress) == false )
		{
			Kernel::Instance().GetSwapManager().FreeSlot(slot);
			this->ReleaseReclaimPin(pageIndex);
			return false;
		}

		if ( frame.mapCount == 0 )
		{
			Kernel::Instance().GetSwapManager().FreeSlot(slot);
			this->ReleaseReclaimPin(pageIndex);
			return true;
		}

		unsigned int evictedCount = 0;
		while ( List::Empty(&frame.rmapHead) == false )
		{
			ReverseMapEntry* entry =
				LIST_FIRST_ENTRY(&frame.rmapHead, ReverseMapEntry, frameNode);
			if ( entry->owner->EvictPageToSwap(entry->virtualPageIndex, slot) == false )
			{
				if ( evictedCount == 0 )
				{
					Kernel::Instance().GetSwapManager().FreeSlot(slot);
				}
				else
				{
					Kernel::Instance().GetSwapManager().SetSlotReferenceCount(slot,
						evictedCount);
				}
				this->ReleaseReclaimPin(pageIndex);
				return false;
			}
			++evictedCount;
		}
		Kernel::Instance().GetSwapManager().SetSlotReferenceCount(slot,
			evictedCount);
		this->ReleaseReclaimPin(pageIndex);
		X86Assembly::FlushCurrentPageDirectory();
		return true;
	}

	return false;
}

void UserPageManager::ReleaseReclaimPin(unsigned long pageIndex)
{
	if ( pageIndex >= this->GetTotalPageCount() )
	{
		return;
	}

	FrameInfo& frame = this->m_FrameInfo[pageIndex];
	frame.flags &= ~FRAME_FLAG_PINNED;
	if ( frame.mapCount == 0 )
	{
		unsigned long physicalAddress = this->GetPageAddressByIndex(pageIndex);
		if ( this->IsZeroPage(physicalAddress) == false )
		{
			this->FreePage(physicalAddress);
		}
	}
}
