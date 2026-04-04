#include "MemoryDescriptor.h"
#include "Kernel.h"
#include "Machine.h"
#include "Process.h"
#include "Utility.h"
#include "Video.h"
#include "Assembly.h"

namespace
{
	static bool ResolveTextStorage(const Text* text,
		MemoryDescriptor::RegionType regionType,
		const unsigned long*& pageArray,
		unsigned int& maxPageCount,
		unsigned long& fileOffset,
		unsigned long& fileSize)
	{
		if ( text == NULL )
		{
			return false;
		}

		if ( regionType == MemoryDescriptor::REGION_CODE )
		{
			pageArray = text->x_addr;
			maxPageCount = Text::MAX_TEXT_PAGE_COUNT;
			fileOffset = text->x_fileoff;
			fileSize = text->x_filesz;
			return true;
		}

		if ( regionType == MemoryDescriptor::REGION_RODATA )
		{
			pageArray = text->x_roaddr;
			maxPageCount = Text::MAX_RODATA_PAGE_COUNT;
			fileOffset = text->x_rofileoff;
			fileSize = text->x_rofilesz;
			return true;
		}

		return false;
	}

	/*
	 * 将 Text 后备偏移解析为物理地址。
	 * 注意：Text 物理页地址改为 x_addr[] 离散存放，不能再按单基址连续计算。
	 */
	static bool ResolveTextPhysicalAddress(const Text* text,
		MemoryDescriptor::RegionType regionType,
		unsigned long backingOffset,
		unsigned long& textPhysicalAddress)
	{
		if ( text == NULL )
		{
			return false;
		}

		unsigned int pageIndex =
			(unsigned int)(backingOffset / PageManager::PAGE_SIZE);

		const unsigned long* pageArray = NULL;
		unsigned int maxPageCount = 0;
		unsigned long ignoredFileOffset = 0;
		unsigned long ignoredFileSize = 0;
		if ( ResolveTextStorage(text,
				regionType,
				pageArray,
				maxPageCount,
				ignoredFileOffset,
				ignoredFileSize) == false )
		{
			return false;
		}

		if ( pageIndex >= maxPageCount )
		{
			return false;
		}

		if ( pageArray[pageIndex] == 0 )
		{
			return false;
		}

		textPhysicalAddress =
			pageArray[pageIndex] + (backingOffset % PageManager::PAGE_SIZE);
		return true;
	}

	static bool ReadFileBackedPage(Inode* inode,
		unsigned long virtualAddress,
		unsigned long fileOffsetBase,
		unsigned long backingOffset,
		unsigned long fileSize)
	{
		if ( backingOffset >= fileSize )
		{
			return true;
		}

		if ( inode == NULL )
		{
			return false;
		}

		unsigned long readSize = fileSize - backingOffset;
		if ( readSize > PageManager::PAGE_SIZE )
		{
			readSize = PageManager::PAGE_SIZE;
		}

		User& u = Kernel::Instance().GetUser();
		u.u_IOParam.m_Base = (unsigned char*)virtualAddress;
		u.u_IOParam.m_Offset = fileOffsetBase + backingOffset;
		u.u_IOParam.m_Count = readSize;
		inode->ReadI();
		if ( u.u_error != User::NOERROR || u.u_IOParam.m_Count != 0 )
		{
			if ( u.u_error == User::NOERROR )
			{
				u.u_error = User::ENOEXEC;
			}
			return false;
		}

		return true;
	}
}

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
		/* 页目录必须按页分配，供 CR3 使用。 */
		unsigned long pageDirectory =
			kernelPageManager.AllocatePage();
		if ( pageDirectory == 0 )
		{
			Utility::Panic("Out of kernel memory for page directory");
		}
		this->m_PageDirectory = (PageDirectory*)(pageDirectory + Machine::KERNEL_SPACE_START_ADDRESS);
	}

	if ( this->m_UserPageTableArray == NULL )
	{
		if ( USER_PRIVATE_PAGE_TABLE_CNT != 1 )
		{
			Utility::Panic("USER_PRIVATE_PAGE_TABLE_CNT must be 1");
		}

		/* 当前设计只维护 1# 私有用户页表，0# 由 Machine 统一维护。 */
		unsigned long pageTables =
			kernelPageManager.AllocatePage();
		if ( pageTables == 0 )
		{
			Utility::Panic("Out of kernel memory for user page tables");
		}
		this->m_UserPageTableArray = (PageTable*)(pageTables + Machine::KERNEL_SPACE_START_ADDRESS);
	}

	if ( this->m_Regions == NULL )
	{
		this->m_Regions = new Region[MAX_REGION_COUNT];
		if ( this->m_Regions == NULL )
		{
			Utility::Panic("Out of kernel memory for regions");
		}
	}

	if ( this->m_PageInfos == NULL )
	{
		this->m_PageInfos = new PageInfo[USER_PAGE_COUNT];
		if ( this->m_PageInfos == NULL )
		{
			Utility::Panic("Out of kernel memory for page infos");
		}
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
		kernelPageManager.FreePage(
			(unsigned long)this->m_UserPageTableArray - Machine::KERNEL_SPACE_START_ADDRESS);
		this->m_UserPageTableArray = NULL;
	}

	if ( this->m_PageDirectory != NULL )
	{
		kernelPageManager.FreePage(
			(unsigned long)this->m_PageDirectory - Machine::KERNEL_SPACE_START_ADDRESS);
		this->m_PageDirectory = NULL;
	}

	if ( this->m_Regions != NULL )
	{
		delete [] this->m_Regions;
		this->m_Regions = NULL;
	}

	if ( this->m_PageInfos != NULL )
	{
		delete [] this->m_PageInfos;
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
		/*
		 * 仅继承布局与后备信息，不继承对方页帧。
		 * 由 CloneResidentPagesFrom 决定是否复制/共享物理页。
		 */
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

	UserPageManager& userPageManager = Kernel::Instance().GetUserPageManager();
	bool parentPteUpdated = false;

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
			/* runtime 低地址页固定映射到物理 0（历史兼容语义）。 */
			dstPage.state = PAGE_STATE_RESIDENT;
			dstPage.frameAddress = 0;
			this->MapPage(0, 0, true);
			continue;
		}

		if ( region.backing.type == BACKING_SHARED_TEXT ||
			(region.prot & PROT_WRITE) == 0 )
		{
			/* fork 仅复制可写页；共享正文与其余只读页统一保留为 RESERVED。 */
			dstPage.state = PAGE_STATE_RESERVED;
			dstPage.frameAddress = 0;
			continue;
		}

		if ( srcPage.frameAddress == 0 )
		{
			return false;
		}

		unsigned int tableIndex =
			i / PageTable::ENTRY_CNT_PER_PAGETABLE;
		unsigned int entryIndex =
			i % PageTable::ENTRY_CNT_PER_PAGETABLE;
		PageTable* parentTable = other.GetUserPageTableByIndex(tableIndex);
		if ( parentTable == NULL ||
			parentTable->m_Entrys[entryIndex].m_Present == 0 )
		{
			return false;
		}

		if ( userPageManager.ShareAsCopyOnWrite(srcPage.frameAddress) == false )
		{
			return false;
		}

		parentTable->m_Entrys[entryIndex].m_ReadWriter = 0;
		parentPteUpdated = true;

		dstPage.state = PAGE_STATE_RESIDENT;
		dstPage.frameAddress = srcPage.frameAddress;
		/* fork 后父子先共享只读页，首次写入时由 COW 分裂。 */
		this->MapPage(virtualAddress, srcPage.frameAddress, false);
	}

	if ( parentPteUpdated )
	{
		/* 父进程当前仍在运行，更新其 RW 位后需刷新 TLB。 */
		X86Assembly::FlushCurrentPageDirectory();
	}

	return true;
}

void MemoryDescriptor::ConfigureExecFileBacking(unsigned long virtualBase,
	unsigned long fileOffset,
	unsigned long fileSize,
	Inode* inode)
{
	if ( this->m_Regions == NULL || this->m_PageInfos == NULL )
	{
		return;
	}

	for ( unsigned int i = 0; i < this->m_RegionCount; ++i )
	{
		Region& region = this->m_Regions[i];
		if ( region.backing.type != BACKING_EXEC_FILE )
		{
			continue;
		}

		region.backing.inode = inode;
		region.backing.fileOffset = fileOffset;
		region.backing.validBytes = fileSize;

		if ( region.start == region.end )
		{
			continue;
		}

		unsigned int startPage = this->AddressToPageIndex(region.start);
		unsigned int endPage = this->AddressToPageIndex(region.end - 1);
		for ( unsigned int pageIndex = startPage; pageIndex <= endPage; ++pageIndex )
		{
			unsigned long pageStart =
				USER_SPACE_START + pageIndex * PageManager::PAGE_SIZE;
			if ( pageStart < virtualBase )
			{
				this->m_PageInfos[pageIndex].backingOffset = 0;
			}
			else
			{
				this->m_PageInfos[pageIndex].backingOffset = pageStart - virtualBase;
			}
		}
	}
}

bool MemoryDescriptor::ConfigureExecutableLayout(unsigned long entryPoint,
												 unsigned long codeStart,
												 unsigned long codeSize,
												 unsigned long dataStart,
												 unsigned long dataSize,
											 unsigned long rodataStart,
											 unsigned long rodataSize,
											 unsigned long bssStart,
											 unsigned long bssSize,
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
	unsigned long roProtectStart = 0;
	unsigned long roProtectEnd = 0;
	unsigned long bssProtectStart = 0;
	unsigned long bssProtectEnd = 0;

	if ( rodataSize != 0 )
	{
		/*
		 * 规则：段边界先按页对齐再标记。
		 * rodata 区域采用 [AlignDown(start), AlignUp(end))，
		 * 再裁剪到 data 区间内。
		 */
		unsigned long candidateStart = AlignDown(rodataStart);
		unsigned long candidateEnd = AlignUp(rodataStart + rodataSize);
		if ( candidateStart < dataStart )
		{
			candidateStart = dataStart;
		}
		if ( candidateEnd > dataEnd )
		{
			candidateEnd = dataEnd;
		}
		if ( candidateStart < candidateEnd )
		{
			roProtectStart = candidateStart;
			roProtectEnd = candidateEnd;
		}
	}

	if ( bssSize != 0 )
	{
		/*
		 * 规则：段边界先按页对齐再标记。
		 * bss 区域采用 [AlignDown(start), AlignUp(end))，
		 * 再裁剪到 data 区间内。
		 */
		unsigned long candidateStart = AlignDown(bssStart);
		unsigned long candidateEnd = AlignUp(bssStart + bssSize);
		if ( candidateStart < dataStart )
		{
			candidateStart = dataStart;
		}
		if ( candidateEnd > dataEnd )
		{
			candidateEnd = dataEnd;
		}
		if ( candidateStart < candidateEnd )
		{
			bssProtectStart = candidateStart;
			bssProtectEnd = candidateEnd;
		}
	}

	/*
	 * 区域顺序固定为 runtime -> code -> [data/rodata/bss] -> heap -> stack，
	 * 便于后续 CheckUserSpace 用“相邻不重叠”规则直接校验。
	 */

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

	struct SpecialRange
	{
		unsigned long start;
		unsigned long end;
		RegionType type;
	};

	SpecialRange ranges[2];
	unsigned int rangeCount = 0;
	if ( roProtectStart < roProtectEnd )
	{
		ranges[rangeCount].start = roProtectStart;
		ranges[rangeCount].end = roProtectEnd;
		ranges[rangeCount].type = REGION_RODATA;
		++rangeCount;
	}
	if ( bssProtectStart < bssProtectEnd )
	{
		ranges[rangeCount].start = bssProtectStart;
		ranges[rangeCount].end = bssProtectEnd;
		ranges[rangeCount].type = REGION_BSS;
		++rangeCount;
	}

	for ( unsigned int i = 0; i + 1 < rangeCount; ++i )
	{
		for ( unsigned int j = i + 1; j < rangeCount; ++j )
		{
			if ( ranges[j].start < ranges[i].start )
			{
				SpecialRange temp = ranges[i];
				ranges[i] = ranges[j];
				ranges[j] = temp;
			}
		}
	}

	bool hasDataRegion = false;
	unsigned long cursor = dataStart;
	for ( unsigned int i = 0; i < rangeCount; ++i )
	{
		unsigned long start = ranges[i].start;
		unsigned long end = ranges[i].end;
		if ( end <= cursor )
		{
			continue;
		}
		if ( start < cursor )
		{
			start = cursor;
		}

		if ( cursor < start )
		{
			if ( this->AddRegion(REGION_DATA, cursor, start,
					PROT_READ | PROT_WRITE | PROT_USER,
					PAGE_FLAG_NONE,
					BACKING_EXEC_FILE) == false )
			{
				return false;
			}
			hasDataRegion = true;
		}

		if ( ranges[i].type == REGION_RODATA )
		{
			if ( this->AddRegion(REGION_RODATA, start, end,
					PROT_READ | PROT_USER, PAGE_FLAG_NONE, BACKING_SHARED_TEXT) == false )
			{
				return false;
			}
		}
		else
		{
			if ( this->AddRegion(REGION_BSS, start, end,
					PROT_READ | PROT_WRITE | PROT_USER,
					PAGE_FLAG_NONE,
					BACKING_ZERO) == false )
			{
				return false;
			}
		}

		cursor = end;
	}

	if ( cursor < dataEnd )
	{
		if ( this->AddRegion(REGION_DATA, cursor, dataEnd,
				PROT_READ | PROT_WRITE | PROT_USER, PAGE_FLAG_NONE, BACKING_EXEC_FILE) == false )
		{
			return false;
		}
		hasDataRegion = true;
	}

	if ( hasDataRegion == false )
	{
		/* 兼容旧路径：始终保留一个 DATA 锚点，供 GetDataStart()/统计逻辑使用。 */
		if ( this->AddRegion(REGION_DATA, dataStart, dataStart,
				PROT_READ | PROT_WRITE | PROT_USER,
				PAGE_FLAG_NONE,
				BACKING_EXEC_FILE) == false )
		{
			return false;
		}
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

bool MemoryDescriptor::HandlePageFault(unsigned long faultAddress,
	unsigned long stackPointer,
	bool isUserMode,
	unsigned int errorCode)
{
	(void)isUserMode;
	bool pageNotPresent = (errorCode & 0x1) == 0;
	bool writeFault = (errorCode & 0x2) != 0;

	/* 超出用户空间上界，一律不是本描述符负责。 */
	if ( faultAddress >= USER_SPACE_END )
	{
		return false;
	}

	if ( pageNotPresent == false && writeFault )
	{
		return this->HandleCopyOnWriteFault(faultAddress);
	}

	if ( pageNotPresent == false )
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
		/* 栈向下增长时，必须保证与 heap 之间至少保留一页缓冲。 */
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

bool MemoryDescriptor::HandleCopyOnWriteFault(unsigned long faultAddress)
{
	if ( this->m_PageInfos == NULL )
	{
		return false;
	}

	unsigned long virtualAddress = AlignDown(faultAddress);
	if ( virtualAddress >= USER_SPACE_END )
	{
		return false;
	}

	unsigned int pageIndex = this->AddressToPageIndex(virtualAddress);
	PageInfo& pageInfo = this->m_PageInfos[pageIndex];
	if ( pageInfo.state != PAGE_STATE_RESIDENT || pageInfo.regionIndex == 0xffff )
	{
		return false;
	}

	const Region& region = this->m_Regions[pageInfo.regionIndex];
	if ( (region.prot & PROT_WRITE) == 0 ||
		region.backing.type == BACKING_SHARED_TEXT )
	{
		return false;
	}

	unsigned int tableIndex =
		pageIndex / PageTable::ENTRY_CNT_PER_PAGETABLE;
	unsigned int entryIndex =
		pageIndex % PageTable::ENTRY_CNT_PER_PAGETABLE;
	PageTable* table = this->GetUserPageTableByIndex(tableIndex);
	if ( table == NULL || table->m_Entrys[entryIndex].m_Present == 0 )
	{
		return false;
	}

	if ( table->m_Entrys[entryIndex].m_ReadWriter != 0 )
	{
		/* 可写页不应进入 COW 路径。 */
		return false;
	}

	unsigned long oldPage = pageInfo.frameAddress;
	if ( oldPage == 0 )
	{
		return false;
	}

	UserPageManager& userPageManager = Kernel::Instance().GetUserPageManager();
	unsigned short refCount = userPageManager.GetCopyOnWriteRefCount(oldPage);
	if ( refCount == 0 )
	{
		/* 非 COW 页但可写区被只读映射，直接恢复 RW。 */
		table->m_Entrys[entryIndex].m_ReadWriter = 1;
		X86Assembly::FlushCurrentPageDirectory();
		return true;
	}

	if ( refCount == 1 )
	{
		/* 最后一个副本，无需拷贝，直接提权并退出 COW。 */
		table->m_Entrys[entryIndex].m_ReadWriter = 1;
		userPageManager.ClearCopyOnWriteRef(oldPage);
		X86Assembly::FlushCurrentPageDirectory();
		return true;
	}

	unsigned long newPage = userPageManager.AllocatePage();
	if ( newPage == 0 )
	{
		return false;
	}

	/* 仍有多个副本，执行真正的 COW 分裂。 */
	Utility::CopyPage(oldPage, newPage);
	userPageManager.ReleaseCopyOnWriteRef(oldPage);
	pageInfo.frameAddress = newPage;
	this->MapPage(virtualAddress, newPage, true);
	userPageManager.ClearCopyOnWriteRef(newPage);
	X86Assembly::FlushCurrentPageDirectory();
	return true;
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

	/* 引导阶段仍保留 runtime 页的立即映射，避免早期路径缺页依赖。 */
	unsigned int runtimePageIndex = this->AddressToPageIndex(0);
	this->m_PageInfos[runtimePageIndex].state = PAGE_STATE_RESIDENT;
	this->m_PageInfos[runtimePageIndex].frameAddress = 0;
	this->MapPage(0, 0, true);
	return true;
}

bool MemoryDescriptor::MaterializeExecutableImage()
{
	/*
	 * Exec 阶段保持 runtime 低地址页立即映射，避免 0# 用户页表共享语义下
	 * 的跨进程映射污染；其余区域维持按需缺页补页。
	 */
	const Region* runtime = this->FindRegionByType(REGION_RUNTIME);
	if ( runtime != NULL )
	{
		unsigned int runtimePageIndex = this->AddressToPageIndex(runtime->start);
		this->m_PageInfos[runtimePageIndex].state = PAGE_STATE_RESIDENT;
		this->m_PageInfos[runtimePageIndex].frameAddress = 0;
		this->MapPage(runtime->start, 0, true);
	}

	const Region* code = this->FindRegionByType(REGION_CODE);
	if ( code != NULL )
	{
		if ( this->m_Owner == NULL || this->m_Owner->p_textp == NULL )
		{
			return false;
		}
	}

	const Region* stack = this->FindRegionByType(REGION_STACK);
	if ( stack != NULL )
	{
		/* 当前策略：仅用户栈在 Exec 时预分配，其余页全部按需缺页补入。 */
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
	/* 按区域后备类型分派补页策略。 */
	switch ( region.backing.type )
	{
	case BACKING_ZERO:
		ok = this->MapZeroPageForCopyOnWrite(virtualAddress);
		break;
	case BACKING_ANON:
		/* 匿名后备：直接分配清零页。 */
		ok = this->AllocateZeroedPage(virtualAddress);
		break;
	case BACKING_SHARED_TEXT:
	case BACKING_EXEC_FILE:
		{
			bool useSharedText = (region.backing.type == BACKING_SHARED_TEXT);
			unsigned int tableIndex =
				pageIndex / PageTable::ENTRY_CNT_PER_PAGETABLE;
			unsigned int entryIndex =
				pageIndex % PageTable::ENTRY_CNT_PER_PAGETABLE;
			PageTable* table = this->GetUserPageTableByIndex(tableIndex);
			if ( table == NULL )
			{
				return false;
			}

			bool readWrite = (region.prot & PROT_WRITE) != 0;
			if ( readWrite == false &&
				table->m_Entrys[entryIndex].m_ReadWriter != 0 )
			{
				/* 兼容 Relocate 期间对只读段临时开启 RW 的场景。 */
				readWrite = true;
			}

			unsigned long fileOffsetBase = 0;
			unsigned long fileSize = 0;
			Inode* inode = NULL;

			const unsigned long* sharedPageArrayConst = NULL;
			unsigned int maxPageCount = 0;
			unsigned int sharedPageIndex = 0;
			unsigned long textPhysicalAddress = 0;
			bool hasSharedResident = false;

			if ( useSharedText )
			{
				if ( this->m_Owner == NULL || this->m_Owner->p_textp == NULL )
				{
					return false;
				}

				Text* text = this->m_Owner->p_textp;
				inode = text->x_iptr;
				if ( ResolveTextStorage(text,
						region.type,
						sharedPageArrayConst,
						maxPageCount,
						fileOffsetBase,
						fileSize) == false )
				{
					return false;
				}

				sharedPageIndex =
					(unsigned int)(pageInfo.backingOffset / PageManager::PAGE_SIZE);
				if ( sharedPageIndex >= maxPageCount )
				{
					return false;
				}

				hasSharedResident = ResolveTextPhysicalAddress(text,
					region.type,
					pageInfo.backingOffset,
					textPhysicalAddress);
				if ( hasSharedResident )
				{
					ok = this->ShareTextPage(virtualAddress, textPhysicalAddress, readWrite);
					break;
				}
			}
			else
			{
				fileOffsetBase = region.backing.fileOffset;
				fileSize = region.backing.validBytes;
				inode = region.backing.inode;
				if ( inode == NULL && this->m_Owner != NULL &&
					this->m_Owner->p_textp != NULL )
				{
					inode = this->m_Owner->p_textp->x_iptr;
				}
			}

			unsigned long newPage =
				Kernel::Instance().GetUserPageManager().AllocatePage();
			if ( newPage == 0 )
			{
				return false;
			}

			Utility::ZeroPage(newPage);

			/* 先临时映射可写，便于回填文件内容，随后 remap 为目标权限。 */
			this->MapPage(virtualAddress, newPage, true);
			if ( ReadFileBackedPage(inode,
					virtualAddress,
					fileOffsetBase,
					pageInfo.backingOffset,
					fileSize) == false )
			{
				table->m_Entrys[entryIndex].m_Present = 0;
				table->m_Entrys[entryIndex].m_ReadWriter = 0;
				table->m_Entrys[entryIndex].m_UserSupervisor = 1;
				table->m_Entrys[entryIndex].m_PageBaseAddress = 0;
				Kernel::Instance().GetUserPageManager().FreePage(newPage);
				pageInfo.state = PAGE_STATE_RESERVED;
				pageInfo.frameAddress = 0;
				return false;
			}

			if ( useSharedText )
			{
				unsigned long* sharedPageArray = (unsigned long*)sharedPageArrayConst;
				sharedPageArray[sharedPageIndex] = newPage;
				ok = this->ShareTextPage(virtualAddress, newPage, readWrite);
			}
			else
			{
				pageInfo.state = PAGE_STATE_RESIDENT;
				pageInfo.frameAddress = newPage;
				this->MapPage(virtualAddress, newPage, readWrite);
				ok = true;
			}
		}
		break;
	default:
		return false;
	}

	if ( ok )
	{
		/* 页表已更新，刷新当前页目录使 CPU 立即可见。 */
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

unsigned int MemoryDescriptor::ExportResidentUserPages(
	UserPageSnapshotEntry* entries,
	unsigned int maxEntries) const
{
	if ( this->m_PageInfos == NULL )
	{
		return 0;
	}

	unsigned int total = 0;
	unsigned int written = 0;

	for ( unsigned int pageIndex = 0; pageIndex < USER_PAGE_COUNT; ++pageIndex )
	{
		if ( this->m_PageInfos[pageIndex].state != PAGE_STATE_RESIDENT )
		{
			continue;
		}

		unsigned int tableIndex =
			pageIndex / PageTable::ENTRY_CNT_PER_PAGETABLE;
		unsigned int entryIndex =
			pageIndex % PageTable::ENTRY_CNT_PER_PAGETABLE;
		PageTable* table = this->GetUserPageTableByIndex(tableIndex);
		if ( table == NULL || table->m_Entrys[entryIndex].m_Present == 0 )
		{
			continue;
		}

		if ( entries != NULL && written < maxEntries )
		{
			unsigned short execFlag = 0;
			if ( this->m_PageInfos[pageIndex].regionIndex != 0xffff )
			{
				const Region& region =
					this->m_Regions[this->m_PageInfos[pageIndex].regionIndex];
				if ( (region.prot & PROT_EXEC) != 0 )
				{
					execFlag = 0x8;
				}
			}

			entries[written].pageIndex = (unsigned short)pageIndex;
			entries[written].flags =
				(table->m_Entrys[entryIndex].m_Present ? 0x1 : 0) |
				(table->m_Entrys[entryIndex].m_ReadWriter ? 0x2 : 0) |
				(table->m_Entrys[entryIndex].m_UserSupervisor ? 0x4 : 0) |
				execFlag;
			entries[written].pageBaseAddress =
				table->m_Entrys[entryIndex].m_PageBaseAddress;
			++written;
		}

		++total;
	}

	return total;
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
		/* 收缩堆：释放已驻留页并清空对应 PageInfo。 */
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
		/* 扩展堆：只保留页元数据，不立即分配物理页。 */
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

	unsigned long newStart = stack->start - PageManager::PAGE_SIZE;
	stack->start = newStart;

	if ( this->m_PageInfos == NULL )
	{
		return;
	}

	unsigned int regionIndex = (unsigned int)(stack - this->m_Regions);
	unsigned int newPageIndex = this->AddressToPageIndex(newStart);
	/* 扩栈仅登记为 RESERVED，首次访问时再由 EnsurePagePresent 分配物理页。 */
	this->m_PageInfos[newPageIndex].state = PAGE_STATE_RESERVED;
	this->m_PageInfos[newPageIndex].flags = stack->flags;
	this->m_PageInfos[newPageIndex].regionIndex = (unsigned short)regionIndex;
	this->m_PageInfos[newPageIndex].frameAddress = 0;
	this->m_PageInfos[newPageIndex].backingOffset = 0;
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
	/*
	 * 区域元数据按声明顺序追加。
	 * 目前不做插入排序，调用方负责按地址顺序传入。
	 */
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
		if ( this->m_PageInfos[i].state == PAGE_STATE_RESIDENT &&
			this->m_PageInfos[i].regionIndex == regionIndex )
		{
			/* 已驻留页保留现状，只更新标记。 */
			this->m_PageInfos[i].flags = region.flags;
			continue;
		}

		/* 未驻留页进入 RESERVED，等待缺页按需物化。 */
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

	unsigned long newPage = Kernel::Instance().GetUserPageManager().AllocatePage();
	if ( newPage == 0 )
	{
		return false;
	}

	/* 分配后立即清零，避免泄露旧页内容到用户空间。 */
	Utility::ZeroPage(newPage);
	pageInfo.state = PAGE_STATE_RESIDENT;
	pageInfo.frameAddress = newPage;

	bool readWrite = (region.prot & PROT_WRITE) != 0;
	if ( readWrite == false )
	{
		/*
		 * 若调用方在缺页前预置了页表 RW（例如 Relocate 临时写 rodata），
		 * 则本次映射继承该位，装载完成后再由调用方恢复只读。
		 */
		unsigned int tableIndex = pageIndex / PageTable::ENTRY_CNT_PER_PAGETABLE;
		unsigned int entryIndex = pageIndex % PageTable::ENTRY_CNT_PER_PAGETABLE;
		PageTable* table = this->GetUserPageTableByIndex(tableIndex);
		if ( table != NULL && table->m_Entrys[entryIndex].m_ReadWriter != 0 )
		{
			readWrite = true;
		}
	}

	this->MapPage(virtualAddress, newPage, readWrite);
	return true;
}

bool MemoryDescriptor::MapZeroPageForCopyOnWrite(unsigned long virtualAddress)
{
	if ( this->m_PageInfos == NULL )
	{
		return false;
	}

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

	UserPageManager& userPageManager = Kernel::Instance().GetUserPageManager();
	unsigned long zeroPage = userPageManager.GetZeroPageAddress();
	if ( userPageManager.ShareAsCopyOnWrite(zeroPage) == false )
	{
		return false;
	}

	pageInfo.state = PAGE_STATE_RESIDENT;
	pageInfo.frameAddress = zeroPage;
	/* BACKING_ZERO 初次映射保持只读，后续写访问进入 COW 分裂。 */
	this->MapPage(virtualAddress, zeroPage, false);
	return true;
}

bool MemoryDescriptor::ShareTextPage(unsigned long virtualAddress,
	unsigned long textPhysicalAddress,
	bool readWrite)
{
	unsigned int pageIndex = this->AddressToPageIndex(virtualAddress);
	PageInfo& pageInfo = this->m_PageInfos[pageIndex];
	pageInfo.state = PAGE_STATE_RESIDENT;
	pageInfo.frameAddress = textPhysicalAddress;
	/* 共享页不分配新物理内存，只建立页表映射。 */
	this->MapPage(virtualAddress, textPhysicalAddress, readWrite);
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
		/* COW 页按引用计数回收；非 COW 页直接释放。 */
		UserPageManager& userPageManager = Kernel::Instance().GetUserPageManager();
		unsigned short cowRef =
			userPageManager.GetCopyOnWriteRefCount(pageInfo.frameAddress);
		if ( cowRef != 0 )
		{
			unsigned short remain =
				userPageManager.ReleaseCopyOnWriteRef(pageInfo.frameAddress);
			if ( remain == 0 )
			{
				userPageManager.FreePage(pageInfo.frameAddress);
			}
		}
		else
		{
			userPageManager.FreePage(pageInfo.frameAddress);
		}
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
		bool readWrite = (region.prot & PROT_WRITE) != 0;
		if ( readWrite )
		{
			UserPageManager& userPageManager = Kernel::Instance().GetUserPageManager();
			if ( userPageManager.GetCopyOnWriteRefCount(this->m_PageInfos[i].frameAddress) != 0 )
			{
				readWrite = false;
			}
		}

		this->MapPage(virtualAddress, this->m_PageInfos[i].frameAddress, readWrite);
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

	/* 统一以用户态可访问映射写入，RW 由调用方决定。 */
	table->m_Entrys[entryIndex].m_Present = 1;
	table->m_Entrys[entryIndex].m_UserSupervisor = 1;
	table->m_Entrys[entryIndex].m_ReadWriter = readWrite ? 1 : 0;
	table->m_Entrys[entryIndex].m_PageBaseAddress =
		physicalAddress / PageManager::PAGE_SIZE;
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
