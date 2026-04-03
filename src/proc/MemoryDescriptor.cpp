#include "MemoryDescriptor.h"
#include "Kernel.h"
#include "Machine.h"
#include "Process.h"
#include "ProcessManager.h"
#include "Utility.h"
#include "Video.h"
#include "Assembly.h"

MemoryDescriptor::MemoryDescriptor()
{
	this->m_Owner = NULL;
	this->m_PageDirectory = NULL;
	this->m_UserPageTableArray = NULL;
	this->m_Regions = NULL;
	this->m_PageInfos = NULL;
	this->m_UsesKernelAddressSpace = false;
	this->Reset();
}

MemoryDescriptor::~MemoryDescriptor()
{
}

void MemoryDescriptor::Attach(Process* owner)
{
	this->m_Owner = owner;
}

void MemoryDescriptor::Reset()
{
	this->ResetLayout();
	this->ClearPageInfos();
	this->ClearPageTables();
	if ( this->m_UsesKernelAddressSpace == false )
	{
		this->ClearPageDirectory();
	}
}

void MemoryDescriptor::ResetLayout()
{
	this->m_RegionCount = 0;
	this->m_EntryPoint = 0;
	this->m_HeapBase = 0;
	this->m_HeapBreak = 0;
	this->m_StackTop = USER_SPACE_END;

	if ( this->m_Regions == NULL )
	{
		return;
	}

	for ( unsigned int i = 0; i < MAX_REGION_COUNT; ++i )
	{
		this->m_Regions[i].start = 0;
		this->m_Regions[i].end = 0;
		this->m_Regions[i].prot = 0;
		this->m_Regions[i].flags = 0;
		this->m_Regions[i].type = REGION_INVALID;
		this->m_Regions[i].backing.type = BACKING_NONE;
		this->m_Regions[i].backing.inode = NULL;
		this->m_Regions[i].backing.text = NULL;
		this->m_Regions[i].backing.fileOffset = 0;
		this->m_Regions[i].backing.validBytes = 0;
	}
}

void MemoryDescriptor::ClearPageInfos()
{
	if ( this->m_PageInfos == NULL )
	{
		return;
	}

	for ( unsigned int i = 0; i < USER_PAGE_COUNT; ++i )
	{
		this->m_PageInfos[i].state = PAGE_STATE_FREE;
		this->m_PageInfos[i].flags = PAGE_FLAG_NONE;
		this->m_PageInfos[i].regionIndex = 0xffff;
		this->m_PageInfos[i].frameAddress = 0;
		this->m_PageInfos[i].backingOffset = 0;
	}
}

void MemoryDescriptor::ClearPageTables()
{
	if ( this->m_UserPageTableArray == NULL )
	{
		return;
	}

	for ( unsigned int i = 0; i < USER_PRIVATE_PAGE_TABLE_CNT; ++i )
	{
		for ( unsigned int j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; ++j )
		{
			this->m_UserPageTableArray[i].m_Entrys[j].m_Present = 0;
			this->m_UserPageTableArray[i].m_Entrys[j].m_ReadWriter = 0;
			this->m_UserPageTableArray[i].m_Entrys[j].m_UserSupervisor = 1;
			this->m_UserPageTableArray[i].m_Entrys[j].m_PageBaseAddress = 0;
		}
	}
}

void MemoryDescriptor::ClearPageDirectory()
{
	if ( this->m_PageDirectory == NULL )
	{
		return;
	}

	for ( unsigned int i = 0; i < 1024; ++i )
	{
		this->m_PageDirectory->m_Entrys[i].m_UserSupervisor = 0;
		this->m_PageDirectory->m_Entrys[i].m_Present = 0;
		this->m_PageDirectory->m_Entrys[i].m_ReadWriter = 0;
		this->m_PageDirectory->m_Entrys[i].m_PageTableBaseAddress = 0;
	}
}

void MemoryDescriptor::Initialize()
{
	KernelPageManager& kernelPageManager = Kernel::Instance().GetKernelPageManager();

	if ( this->m_UsesKernelAddressSpace )
	{
		this->m_UsesKernelAddressSpace = false;
		this->m_PageDirectory = NULL;
	}

	if ( this->m_PageDirectory == NULL )
	{
		unsigned long pageDirectory = kernelPageManager.AllocMemory(sizeof(PageDirectory));
		if ( pageDirectory == 0 )
		{
			Utility::Panic("Out of kernel memory for page directory");
		}
		this->m_PageDirectory = (PageDirectory*)(pageDirectory + Machine::KERNEL_SPACE_START_ADDRESS);
	}

	if ( this->m_UserPageTableArray == NULL )
	{
		unsigned long pageTables = kernelPageManager.AllocMemory(sizeof(PageTable) * USER_PRIVATE_PAGE_TABLE_CNT);
		if ( pageTables == 0 )
		{
			Utility::Panic("Out of kernel memory for user page tables");
		}
		this->m_UserPageTableArray = (PageTable*)(pageTables + Machine::KERNEL_SPACE_START_ADDRESS);
	}

	if ( this->m_Regions == NULL )
	{
		unsigned long regions = kernelPageManager.AllocMemory(sizeof(Region) * MAX_REGION_COUNT);
		if ( regions == 0 )
		{
			Utility::Panic("Out of kernel memory for regions");
		}
		this->m_Regions = (Region*)(regions + Machine::KERNEL_SPACE_START_ADDRESS);
	}

	if ( this->m_PageInfos == NULL )
	{
		unsigned long pageInfos = kernelPageManager.AllocMemory(sizeof(PageInfo) * USER_PAGE_COUNT);
		if ( pageInfos == 0 )
		{
			Utility::Panic("Out of kernel memory for page infos");
		}
		this->m_PageInfos = (PageInfo*)(pageInfos + Machine::KERNEL_SPACE_START_ADDRESS);
	}

	this->Reset();
}

void MemoryDescriptor::Release()
{
	KernelPageManager& kernelPageManager = Kernel::Instance().GetKernelPageManager();

	if ( this->m_UsesKernelAddressSpace )
	{
		this->m_PageDirectory = NULL;
		this->m_UserPageTableArray = NULL;
		this->m_Regions = NULL;
		this->m_PageInfos = NULL;
		this->m_UsesKernelAddressSpace = false;
		this->ResetLayout();
		this->ClearPageInfos();
		return;
	}

	if ( this->m_UserPageTableArray != NULL )
	{
		kernelPageManager.FreeMemory(sizeof(PageTable) * USER_PRIVATE_PAGE_TABLE_CNT,
			(unsigned long)this->m_UserPageTableArray - Machine::KERNEL_SPACE_START_ADDRESS);
		this->m_UserPageTableArray = NULL;
	}

	if ( this->m_PageDirectory != NULL )
	{
		kernelPageManager.FreeMemory(sizeof(PageDirectory),
			(unsigned long)this->m_PageDirectory - Machine::KERNEL_SPACE_START_ADDRESS);
		this->m_PageDirectory = NULL;
	}

	if ( this->m_Regions != NULL )
	{
		kernelPageManager.FreeMemory(sizeof(Region) * MAX_REGION_COUNT,
			(unsigned long)this->m_Regions - Machine::KERNEL_SPACE_START_ADDRESS);
		this->m_Regions = NULL;
	}

	if ( this->m_PageInfos != NULL )
	{
		kernelPageManager.FreeMemory(sizeof(PageInfo) * USER_PAGE_COUNT,
			(unsigned long)this->m_PageInfos - Machine::KERNEL_SPACE_START_ADDRESS);
		this->m_PageInfos = NULL;
	}

	this->ResetLayout();
	this->ClearPageInfos();
}

void MemoryDescriptor::UseKernelAddressSpace(PageDirectory* pageDirectory)
{
	this->Release();
	this->m_PageDirectory = pageDirectory;
	this->m_UserPageTableArray = NULL;
	this->m_Regions = NULL;
	this->m_PageInfos = NULL;
	this->m_UsesKernelAddressSpace = true;
	this->ResetLayout();
	this->ClearPageInfos();
}

void MemoryDescriptor::CloneFrom(const MemoryDescriptor& other)
{
	this->ResetLayout();
	this->ClearPageInfos();

	this->m_EntryPoint = other.m_EntryPoint;
	this->m_HeapBase = other.m_HeapBase;
	this->m_HeapBreak = other.m_HeapBreak;
	this->m_StackTop = other.m_StackTop;

	this->m_RegionCount = other.m_RegionCount;
	for ( unsigned int i = 0; i < other.m_RegionCount; ++i )
	{
		this->m_Regions[i] = other.m_Regions[i];
	}

	for ( unsigned int i = 0; i < USER_PAGE_COUNT; ++i )
	{
		this->m_PageInfos[i].state = other.m_PageInfos[i].state == PAGE_STATE_FREE ? PAGE_STATE_FREE : PAGE_STATE_RESERVED;
		this->m_PageInfos[i].flags = other.m_PageInfos[i].flags;
		this->m_PageInfos[i].regionIndex = other.m_PageInfos[i].regionIndex;
		this->m_PageInfos[i].frameAddress = 0;
		this->m_PageInfos[i].backingOffset = other.m_PageInfos[i].backingOffset;
	}

	this->ClearPageTables();
	if ( this->m_UsesKernelAddressSpace == false )
	{
		this->ClearPageDirectory();
	}
}

bool MemoryDescriptor::CloneResidentPagesFrom(const MemoryDescriptor& other)
{
	if ( this->m_PageInfos == NULL || this->m_UserPageTableArray == NULL )
	{
		return false;
	}

	for ( unsigned int i = 0; i < USER_PAGE_COUNT; ++i )
	{
		if ( other.m_PageInfos == NULL || other.m_PageInfos[i].state != PAGE_STATE_RESIDENT )
		{
			continue;
		}

		PageInfo& dstPage = this->m_PageInfos[i];
		const PageInfo& srcPage = other.m_PageInfos[i];
		const Region& region = other.m_Regions[srcPage.regionIndex];
		unsigned long virtualAddress = USER_SPACE_START + i * PageManager::PAGE_SIZE;

		if ( region.type == REGION_RUNTIME )
		{
			dstPage.state = PAGE_STATE_RESIDENT;
			dstPage.frameAddress = 0;
			this->MapPage(0, 0, true);
			continue;
		}

		if ( region.backing.type == BACKING_SHARED_TEXT && other.m_Owner != NULL &&
			other.m_Owner->p_textp != NULL )
		{
			if ( this->ShareTextPage(virtualAddress,
				other.m_Owner->p_textp->x_caddr + srcPage.backingOffset) == false )
			{
				return false;
			}
			dstPage.state = PAGE_STATE_RESIDENT;
			dstPage.frameAddress = other.m_Owner->p_textp->x_caddr + srcPage.backingOffset;
			continue;
		}

		unsigned long newPage = Kernel::Instance().GetUserPageManager().AllocMemory(PageManager::PAGE_SIZE);
		if ( newPage == 0 )
		{
			return false;
		}

		Utility::CopyPage(srcPage.frameAddress, newPage);
		dstPage.state = PAGE_STATE_RESIDENT;
		dstPage.frameAddress = newPage;
		this->MapPage(virtualAddress, newPage, (region.prot & PROT_WRITE) != 0);
	}

	return true;
}

bool MemoryDescriptor::ConfigureExecutableLayout(unsigned long entryPoint,
												 unsigned long codeStart,
												 unsigned long codeSize,
												 unsigned long dataStart,
												 unsigned long dataSize,
												 unsigned long stackSize)
{
	this->ResetLayout();
	this->ClearPageInfos();
	this->ClearPageTables();

	this->m_EntryPoint = entryPoint;
	this->m_StackTop = USER_SPACE_END;

	unsigned long codeEnd = AlignUp(codeStart + codeSize);
	unsigned long dataEnd = AlignUp(dataStart + dataSize);
	unsigned long stackBottom = AlignDown(USER_SPACE_END - stackSize);

	if ( this->AddRegion(REGION_RUNTIME, 0, PageManager::PAGE_SIZE,
			PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER, PAGE_FLAG_NONE, BACKING_ANON) == false )
	{
		return false;
	}

	if ( this->AddRegion(REGION_CODE, codeStart, codeEnd,
			PROT_READ | PROT_EXEC | PROT_USER, PAGE_FLAG_NONE, BACKING_SHARED_TEXT) == false )
	{
		return false;
	}

	if ( this->AddRegion(REGION_DATA, dataStart, dataEnd,
			PROT_READ | PROT_WRITE | PROT_USER, PAGE_FLAG_NONE, BACKING_EXEC_FILE) == false )
	{
		return false;
	}

	this->m_HeapBase = dataEnd;
	this->m_HeapBreak = dataEnd;

	if ( this->AddRegion(REGION_HEAP, this->m_HeapBase, this->m_HeapBreak,
			PROT_READ | PROT_WRITE | PROT_USER, PAGE_FLAG_NONE, BACKING_ZERO) == false )
	{
		return false;
	}

	if ( this->AddRegion(REGION_STACK, stackBottom, USER_SPACE_END,
			PROT_READ | PROT_WRITE | PROT_USER, PAGE_FLAG_GROWSDOWN, BACKING_ZERO) == false )
	{
		return false;
	}

	return this->CheckUserSpace();
}

bool MemoryDescriptor::CheckUserSpace() const
{
	for ( unsigned int i = 0; i < this->m_RegionCount; ++i )
	{
		const Region& region = this->m_Regions[i];
		if ( region.start > region.end || region.end > USER_SPACE_END )
		{
			return false;
		}
	}

	for ( unsigned int i = 1; i < this->m_RegionCount; ++i )
	{
		if ( this->m_Regions[i - 1].end > this->m_Regions[i].start )
		{
			return false;
		}
	}

	return true;
}

bool MemoryDescriptor::HandlePageFault(unsigned long faultAddress, unsigned long stackPointer, bool isUserMode)
{
	if ( faultAddress >= USER_SPACE_END )
	{
		return false;
	}

	Region* region = this->FindRegion(faultAddress);
	Region* stack = this->FindRegionByType(REGION_STACK);

	/*
	 * 无论缺页发生在用户态还是内核态，只要访问的是当前进程用户地址空间中的合法地址，
	 * 都允许 MemoryDescriptor 负责补页。内核态常见于系统调用中直接拷贝用户缓冲区。
	 */
	if ( region == NULL && stack != NULL &&
		faultAddress >= stackPointer - 8 && faultAddress < stack->start )
	{
		if ( stack->start <= this->m_HeapBreak + PageManager::PAGE_SIZE )
		{
			return false;
		}

		this->GrowStackByPage();
		region = this->FindRegion(faultAddress);
	}

	if ( region == NULL )
	{
		return false;
	}

	return this->EnsurePagePresent(faultAddress);
}

bool MemoryDescriptor::MaterializeBootstrapStack()
{
	if ( this->m_RegionCount != 0 )
	{
		return true;
	}

	this->ResetLayout();
	this->ClearPageInfos();
	this->ClearPageTables();

	this->m_EntryPoint = 0;
	this->m_HeapBase = PageManager::PAGE_SIZE;
	this->m_HeapBreak = PageManager::PAGE_SIZE;
	this->m_StackTop = USER_SPACE_END;

	if ( this->AddRegion(REGION_RUNTIME, 0, PageManager::PAGE_SIZE,
			PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER, PAGE_FLAG_NONE, BACKING_ANON) == false )
	{
		return false;
	}

	if ( this->AddRegion(REGION_HEAP, this->m_HeapBase, this->m_HeapBreak,
			PROT_READ | PROT_WRITE | PROT_USER, PAGE_FLAG_NONE, BACKING_ZERO) == false )
	{
		return false;
	}

	if ( this->AddRegion(REGION_STACK, USER_SPACE_END - PageManager::PAGE_SIZE, USER_SPACE_END,
			PROT_READ | PROT_WRITE | PROT_USER, PAGE_FLAG_GROWSDOWN, BACKING_ZERO) == false )
	{
		return false;
	}

	if ( this->AllocateZeroedPage(USER_SPACE_END - PageManager::PAGE_SIZE) == false )
	{
		return false;
	}

	unsigned int runtimePageIndex = this->AddressToPageIndex(0);
	this->m_PageInfos[runtimePageIndex].state = PAGE_STATE_RESIDENT;
	this->m_PageInfos[runtimePageIndex].frameAddress = 0;
	this->MapPage(0, 0, true);
	return true;
}

bool MemoryDescriptor::MaterializeExecutableImage(unsigned long textPhysicalAddress)
{
	if ( this->FindRegionByType(REGION_RUNTIME) != NULL )
	{
		unsigned int runtimePageIndex = this->AddressToPageIndex(0);
		this->m_PageInfos[runtimePageIndex].state = PAGE_STATE_RESIDENT;
		this->m_PageInfos[runtimePageIndex].frameAddress = 0;
		this->MapPage(0, 0, true);
	}

	const Region* code = this->FindRegionByType(REGION_CODE);
	if ( code != NULL )
	{
		for ( unsigned long va = code->start; va < code->end; va += PageManager::PAGE_SIZE )
		{
			if ( this->ShareTextPage(va, textPhysicalAddress + (va - code->start)) == false )
			{
				return false;
			}
		}
	}

	const Region* data = this->FindRegionByType(REGION_DATA);
	if ( data != NULL )
	{
		for ( unsigned long va = data->start; va < data->end; va += PageManager::PAGE_SIZE )
		{
			if ( this->AllocateZeroedPage(va) == false )
			{
				return false;
			}
		}
	}

	const Region* stack = this->FindRegionByType(REGION_STACK);
	if ( stack != NULL )
	{
		for ( unsigned long va = stack->start; va < stack->end; va += PageManager::PAGE_SIZE )
		{
			if ( this->AllocateZeroedPage(va) == false )
			{
				return false;
			}
		}
	}

	return true;
}

bool MemoryDescriptor::EnsurePagePresent(unsigned long faultAddress)
{
	if ( this->m_PageInfos == NULL )
	{
		return false;
	}

	unsigned long virtualAddress = AlignDown(faultAddress);
	unsigned int pageIndex = this->AddressToPageIndex(virtualAddress);
	PageInfo& pageInfo = this->m_PageInfos[pageIndex];
	if ( pageInfo.state == PAGE_STATE_RESIDENT )
	{
		return true;
	}
	if ( pageInfo.state != PAGE_STATE_RESERVED || pageInfo.regionIndex == 0xffff )
	{
		return false;
	}

	const Region& region = this->m_Regions[pageInfo.regionIndex];
	bool ok = false;
	switch ( region.backing.type )
	{
	case BACKING_ZERO:
	case BACKING_ANON:
		ok = this->AllocateZeroedPage(virtualAddress);
		break;
	case BACKING_SHARED_TEXT:
		if ( this->m_Owner == NULL || this->m_Owner->p_textp == NULL )
		{
			return false;
		}
		ok = this->ShareTextPage(virtualAddress,
			this->m_Owner->p_textp->x_caddr + pageInfo.backingOffset);
		break;
	case BACKING_EXEC_FILE:
		ok = this->AllocateZeroedPage(virtualAddress);
		break;
	default:
		return false;
	}

	if ( ok )
	{
		X86Assembly::FlushCurrentPageDirectory();
	}
	return ok;
}

void MemoryDescriptor::ReleaseResidentPages(bool releaseSharedText)
{
	if ( this->m_PageInfos == NULL )
	{
		return;
	}

	for ( unsigned int i = 0; i < USER_PAGE_COUNT; ++i )
	{
		this->FreePageInfo(this->m_PageInfos[i], releaseSharedText);
	}
	this->ClearPageTables();
}

void MemoryDescriptor::BuildPageTablesForImage()
{
	if ( this->m_UserPageTableArray == NULL )
	{
		return;
	}

	this->ClearPageTables();
	this->RemapResidentPages();
}

void MemoryDescriptor::Activate()
{
	if ( this->m_PageDirectory == NULL || this->m_UsesKernelAddressSpace )
	{
		return;
	}

	Machine::Instance().WriteUserPageDirectoryEntry(this->m_PageDirectory, this->m_UserPageTableArray);
}

void MemoryDescriptor::DisplayPageTable()
{
	if ( this->m_UserPageTableArray == NULL )
	{
		Diagnose::Write("Process PT: <empty>\n");
		return;
	}

	Diagnose::Write("Process PT:");
	for ( unsigned int i = 0; i < USER_PAGE_TABLE_CNT; ++i )
	{
		PageTable* table = this->GetUserPageTableByIndex(i);
		if ( table == NULL )
		{
			continue;
		}
		for ( unsigned int j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; ++j )
		{
			if ( table->m_Entrys[j].m_Present == 1 )
			{
				Diagnose::Write("<%d,%x>  ", i * 1024 + j,
					table->m_Entrys[j].m_PageBaseAddress);
			}
		}
	}
	Diagnose::Write("\n");
}

bool MemoryDescriptor::SetHeapBreak(unsigned long newBreak)
{
	Region* heap = this->FindRegionByType(REGION_HEAP);
	if ( heap == NULL )
	{
		return false;
	}

	unsigned long alignedBreak = AlignUp(newBreak);
	if ( alignedBreak < this->m_HeapBase )
	{
		return false;
	}

	Region* stack = this->FindRegionByType(REGION_STACK);
	if ( alignedBreak > stack->start )
	{
		return false;
	}

	unsigned long oldEnd = heap->end;
	if ( alignedBreak < oldEnd )
	{
		for ( unsigned long va = alignedBreak; va < oldEnd; va += PageManager::PAGE_SIZE )
		{
			unsigned int pageIndex = this->AddressToPageIndex(va);
			this->FreePageInfo(this->m_PageInfos[pageIndex], false);
			this->m_PageInfos[pageIndex].state = PAGE_STATE_FREE;
			this->m_PageInfos[pageIndex].regionIndex = 0xffff;
			this->m_PageInfos[pageIndex].backingOffset = 0;
		}
	}

	unsigned long oldAlignedEnd = AlignUp(oldEnd);
	heap->end = alignedBreak;
	this->m_HeapBreak = alignedBreak;

	if ( alignedBreak > oldAlignedEnd )
	{
		unsigned int startPage = this->AddressToPageIndex(oldAlignedEnd);
		unsigned int endPage = this->AddressToPageIndex(alignedBreak - 1);
		for ( unsigned int i = startPage; i <= endPage; ++i )
		{
			this->m_PageInfos[i].state = PAGE_STATE_RESERVED;
			this->m_PageInfos[i].flags = heap->flags;
			this->m_PageInfos[i].regionIndex = (unsigned short)(heap - this->m_Regions);
			this->m_PageInfos[i].frameAddress = 0;
			this->m_PageInfos[i].backingOffset =
				(i - this->AddressToPageIndex(heap->start)) * PageManager::PAGE_SIZE;
		}
	}

	return true;
}

void MemoryDescriptor::GrowStackByPage()
{
	Region* stack = this->FindRegionByType(REGION_STACK);
	if ( stack == NULL )
	{
		return;
	}

	stack->start -= PageManager::PAGE_SIZE;
	this->ReservePagesForRegion((unsigned int)(stack - this->m_Regions));
}

PageDirectory* MemoryDescriptor::GetPageDirectoryPointer() const
{
	return this->m_PageDirectory;
}

PageTable* MemoryDescriptor::GetUserPageTableArray() const
{
	return this->m_UserPageTableArray;
}

PageTable* MemoryDescriptor::GetUserPageTableByIndex(unsigned int index) const
{
	if ( index >= USER_PAGE_TABLE_CNT )
	{
		return NULL;
	}

	if ( index == 0 )
	{
		return Machine::Instance().GetUserPageTableArray();
	}

	return this->m_UserPageTableArray;
}

bool MemoryDescriptor::HasUserAddressSpace() const
{
	return this->m_UserPageTableArray != NULL;
}

unsigned long MemoryDescriptor::GetEntryPoint() const
{
	return this->m_EntryPoint;
}

unsigned long MemoryDescriptor::GetCodeStart() const
{
	const Region* region = this->FindRegionByType(REGION_CODE);
	return region == NULL ? 0 : region->start;
}

unsigned long MemoryDescriptor::GetCodeSize() const
{
	const Region* region = this->FindRegionByType(REGION_CODE);
	return region == NULL ? 0 : region->end - region->start;
}

unsigned long MemoryDescriptor::GetDataStart() const
{
	const Region* region = this->FindRegionByType(REGION_DATA);
	return region == NULL ? 0 : region->start;
}

unsigned long MemoryDescriptor::GetDataSize() const
{
	const Region* region = this->FindRegionByType(REGION_DATA);
	return region == NULL ? 0 : region->end - region->start;
}

unsigned long MemoryDescriptor::GetHeapBase() const
{
	return this->m_HeapBase;
}

unsigned long MemoryDescriptor::GetHeapBreak() const
{
	return this->m_HeapBreak;
}

unsigned long MemoryDescriptor::GetStackTop() const
{
	return this->m_StackTop;
}

unsigned long MemoryDescriptor::GetStackBottom() const
{
	const Region* region = this->FindRegionByType(REGION_STACK);
	return region == NULL ? USER_SPACE_END : region->start;
}

unsigned long MemoryDescriptor::GetStackSize() const
{
	const Region* region = this->FindRegionByType(REGION_STACK);
	return region == NULL ? 0 : region->end - region->start;
}

unsigned long MemoryDescriptor::GetWritableSize() const
{
	if ( this->GetDataStart() == 0 && this->m_HeapBreak == 0 )
	{
		return this->GetStackSize();
	}

	return (this->m_HeapBreak - this->GetDataStart()) + this->GetStackSize();
}

MemoryDescriptor::Region* MemoryDescriptor::FindRegion(unsigned long address)
{
	for ( unsigned int i = 0; i < this->m_RegionCount; ++i )
	{
		if ( this->m_Regions[i].start <= address && address < this->m_Regions[i].end )
		{
			return &this->m_Regions[i];
		}
	}

	return NULL;
}

const MemoryDescriptor::Region* MemoryDescriptor::FindRegion(unsigned long address) const
{
	for ( unsigned int i = 0; i < this->m_RegionCount; ++i )
	{
		if ( this->m_Regions[i].start <= address && address < this->m_Regions[i].end )
		{
			return &this->m_Regions[i];
		}
	}

	return NULL;
}

unsigned long MemoryDescriptor::AlignDown(unsigned long value)
{
	return value & ~(PageManager::PAGE_SIZE - 1);
}

unsigned long MemoryDescriptor::AlignUp(unsigned long value)
{
	return (value + PageManager::PAGE_SIZE - 1) & ~(PageManager::PAGE_SIZE - 1);
}

bool MemoryDescriptor::AddRegion(RegionType type,
								 unsigned long start,
								 unsigned long end,
								 unsigned int prot,
								 unsigned int flags,
								 BackingType backingType)
{
	if ( start > end || this->m_RegionCount >= MAX_REGION_COUNT )
	{
		return false;
	}

	if ( this->m_Regions == NULL )
	{
		return false;
	}

	unsigned int idx = this->m_RegionCount++;
	this->m_Regions[idx].start = start;
	this->m_Regions[idx].end = end;
	this->m_Regions[idx].prot = prot;
	this->m_Regions[idx].flags = flags;
	this->m_Regions[idx].type = type;
	this->m_Regions[idx].backing.type = backingType;
	this->m_Regions[idx].backing.inode = NULL;
	this->m_Regions[idx].backing.text = NULL;
	this->m_Regions[idx].backing.fileOffset = 0;
	this->m_Regions[idx].backing.validBytes = end - start;
	if ( start != end )
	{
		this->ReservePagesForRegion(idx);
	}
	return true;
}

void MemoryDescriptor::ReservePagesForRegion(unsigned int regionIndex)
{
	if ( this->m_PageInfos == NULL )
	{
		return;
	}

	const Region& region = this->m_Regions[regionIndex];
	unsigned int startPage = this->AddressToPageIndex(region.start);
	unsigned int endPage = this->AddressToPageIndex(region.end - 1);

	for ( unsigned int i = startPage; i <= endPage; ++i )
	{
		this->m_PageInfos[i].state = PAGE_STATE_RESERVED;
		this->m_PageInfos[i].flags = region.flags;
		this->m_PageInfos[i].regionIndex = regionIndex;
		this->m_PageInfos[i].frameAddress = 0;
		this->m_PageInfos[i].backingOffset = (i - startPage) * PageManager::PAGE_SIZE;
	}
}

bool MemoryDescriptor::AllocateZeroedPage(unsigned long virtualAddress)
{
	unsigned int pageIndex = this->AddressToPageIndex(virtualAddress);
	PageInfo& pageInfo = this->m_PageInfos[pageIndex];
	const Region& region = this->m_Regions[pageInfo.regionIndex];
	if ( pageInfo.state == PAGE_STATE_RESIDENT )
	{
		return true;
	}

	unsigned long newPage = Kernel::Instance().GetUserPageManager().AllocMemory(PageManager::PAGE_SIZE);
	if ( newPage == 0 )
	{
		return false;
	}

	Utility::ZeroPage(newPage);
	pageInfo.state = PAGE_STATE_RESIDENT;
	pageInfo.frameAddress = newPage;
	this->MapPage(virtualAddress, newPage, (region.prot & PROT_WRITE) != 0);
	return true;
}

bool MemoryDescriptor::ShareTextPage(unsigned long virtualAddress, unsigned long textPhysicalAddress)
{
	unsigned int pageIndex = this->AddressToPageIndex(virtualAddress);
	PageInfo& pageInfo = this->m_PageInfos[pageIndex];
	pageInfo.state = PAGE_STATE_RESIDENT;
	pageInfo.frameAddress = textPhysicalAddress;
	this->MapPage(virtualAddress, textPhysicalAddress, false);
	return true;
}

void MemoryDescriptor::FreePageInfo(PageInfo& pageInfo, bool releaseSharedText)
{
	if ( pageInfo.state != PAGE_STATE_RESIDENT || pageInfo.regionIndex == 0xffff )
	{
		return;
	}

	const Region& region = this->m_Regions[pageInfo.regionIndex];
	if ( pageInfo.frameAddress != 0 &&
		(region.backing.type != BACKING_SHARED_TEXT || releaseSharedText) )
	{
		Kernel::Instance().GetUserPageManager().FreeMemory(PageManager::PAGE_SIZE, pageInfo.frameAddress);
	}

	pageInfo.state = PAGE_STATE_RESERVED;
	pageInfo.frameAddress = 0;
}

void MemoryDescriptor::RemapResidentPages()
{
	if ( this->m_PageInfos == NULL )
	{
		return;
	}

	for ( unsigned int i = 0; i < USER_PAGE_COUNT; ++i )
	{
		if ( this->m_PageInfos[i].state != PAGE_STATE_RESIDENT ||
			this->m_PageInfos[i].regionIndex == 0xffff )
		{
			continue;
		}

		const Region& region = this->m_Regions[this->m_PageInfos[i].regionIndex];
		unsigned long virtualAddress = USER_SPACE_START + i * PageManager::PAGE_SIZE;
		this->MapPage(virtualAddress, this->m_PageInfos[i].frameAddress,
			(region.prot & PROT_WRITE) != 0);
	}
}

unsigned int MemoryDescriptor::AddressToPageIndex(unsigned long address) const
{
	return (address - USER_SPACE_START) / PageManager::PAGE_SIZE;
}

void MemoryDescriptor::MapPage(unsigned long virtualAddress, unsigned long physicalAddress, bool readWrite)
{
	unsigned int pageIndex = this->AddressToPageIndex(virtualAddress);
	unsigned int tableIndex = pageIndex / PageTable::ENTRY_CNT_PER_PAGETABLE;
	unsigned int entryIndex = pageIndex % PageTable::ENTRY_CNT_PER_PAGETABLE;
	PageTable* table = this->GetUserPageTableByIndex(tableIndex);
	if ( table == NULL )
	{
		return;
	}

	table->m_Entrys[entryIndex].m_Present = 1;
	table->m_Entrys[entryIndex].m_UserSupervisor = 1;
	table->m_Entrys[entryIndex].m_ReadWriter = readWrite ? 1 : 0;
	table->m_Entrys[entryIndex].m_PageBaseAddress =
		physicalAddress / PageManager::PAGE_SIZE;
}

void MemoryDescriptor::MarkRangeResident(unsigned long virtualAddress,
										 unsigned long size,
										 unsigned long physicalAddress,
										 bool readWrite)
{
	if ( size == 0 )
	{
		return;
	}

	unsigned long start = AlignDown(virtualAddress);
	unsigned long end = AlignUp(virtualAddress + size);

	for ( unsigned long va = start, pa = physicalAddress; va < end;
		  va += PageManager::PAGE_SIZE, pa += PageManager::PAGE_SIZE )
	{
		unsigned int pageIndex = this->AddressToPageIndex(va);
		this->m_PageInfos[pageIndex].state = PAGE_STATE_RESIDENT;
		this->m_PageInfos[pageIndex].frameAddress = pa;
		this->MapPage(va, pa, readWrite);
	}
}

MemoryDescriptor::Region* MemoryDescriptor::FindRegionByType(RegionType type)
{
	for ( unsigned int i = 0; i < this->m_RegionCount; ++i )
	{
		if ( this->m_Regions[i].type == type )
		{
			return &this->m_Regions[i];
		}
	}

	return NULL;
}

const MemoryDescriptor::Region* MemoryDescriptor::FindRegionByType(RegionType type) const
{
	for ( unsigned int i = 0; i < this->m_RegionCount; ++i )
	{
		if ( this->m_Regions[i].type == type )
		{
			return &this->m_Regions[i];
		}
	}

	return NULL;
}
