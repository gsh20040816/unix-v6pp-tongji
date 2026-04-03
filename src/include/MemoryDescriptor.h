#ifndef MEMORY_DESCRIPTOR_H
#define MEMORY_DESCRIPTOR_H

#include "PageDirectory.h"
#include "PageTable.h"
#include "INode.h"
#include "Text.h"
#include "PageManager.h"

class Process;

class MemoryDescriptor
{
public:
	static const unsigned long USER_SPACE_START = 0x00000000;
	static const unsigned long USER_SPACE_END = 0x00800000;
	static const unsigned long USER_SPACE_SIZE = USER_SPACE_END - USER_SPACE_START;
	static const unsigned int USER_PAGE_TABLE_CNT = 0x2;
	static const unsigned int USER_PAGE_COUNT = USER_SPACE_SIZE / PageManager::PAGE_SIZE;
	static const unsigned int MAX_REGION_COUNT = 16;

	enum RegionType
	{
		REGION_INVALID = 0,
		REGION_RUNTIME,
		REGION_CODE,
		REGION_RODATA,
		REGION_DATA,
		REGION_BSS,
		REGION_HEAP,
		REGION_STACK
	};

	enum BackingType
	{
		BACKING_NONE = 0,
		BACKING_ZERO,
		BACKING_EXEC_FILE,
		BACKING_SHARED_TEXT,
		BACKING_ANON
	};

	enum PageState
	{
		PAGE_STATE_FREE = 0,
		PAGE_STATE_RESERVED,
		PAGE_STATE_RESIDENT
	};

	enum ProtFlags
	{
		PROT_READ = 0x1,
		PROT_WRITE = 0x2,
		PROT_EXEC = 0x4,
		PROT_USER = 0x8
	};

	enum PageFlags
	{
		PAGE_FLAG_NONE = 0x0,
		PAGE_FLAG_DIRTY = 0x1,
		PAGE_FLAG_ACCESSED = 0x2,
		PAGE_FLAG_GROWSDOWN = 0x4
	};

	struct BackingStore
	{
		BackingType type;
		Inode* inode;
		Text* text;
		unsigned long fileOffset;
		unsigned long validBytes;
	};

	struct Region
	{
		unsigned long start;
		unsigned long end;
		unsigned int prot;
		unsigned int flags;
		RegionType type;
		BackingStore backing;
	};

	struct PageInfo
	{
		PageState state;
		unsigned int flags;
		unsigned short regionIndex;
		unsigned long frameAddress;
		unsigned long backingOffset;
	};

public:
	MemoryDescriptor();
	~MemoryDescriptor();

	void Attach(Process* owner);
	void Reset();
	void Initialize();
	void Release();

	void UseKernelAddressSpace(PageDirectory* pageDirectory);
	void CloneFrom(const MemoryDescriptor& other);
	bool CloneResidentPagesFrom(const MemoryDescriptor& other);

	bool ConfigureExecutableLayout(unsigned long entryPoint,
								   unsigned long codeStart,
								   unsigned long codeSize,
								   unsigned long dataStart,
								   unsigned long dataSize,
								   unsigned long stackSize);

	bool CheckUserSpace() const;
	bool HandlePageFault(unsigned long faultAddress, unsigned long stackPointer, bool isUserMode);
	bool MaterializeBootstrapStack();
	bool MaterializeExecutableImage(unsigned long textPhysicalAddress);
	bool EnsurePagePresent(unsigned long faultAddress);
	void ReleaseResidentPages(bool releaseSharedText);

	void BuildPageTablesForImage();
	void Activate();
	void DisplayPageTable();

	bool SetHeapBreak(unsigned long newBreak);
	void GrowStackByPage();

	PageDirectory* GetPageDirectoryPointer() const;
	PageTable* GetUserPageTableArray() const;
	PageTable* GetUserPageTableByIndex(unsigned int index) const;
	bool HasUserAddressSpace() const;

	unsigned long GetEntryPoint() const;
	unsigned long GetCodeStart() const;
	unsigned long GetCodeSize() const;
	unsigned long GetDataStart() const;
	unsigned long GetDataSize() const;
	unsigned long GetHeapBase() const;
	unsigned long GetHeapBreak() const;
	unsigned long GetStackTop() const;
	unsigned long GetStackBottom() const;
	unsigned long GetStackSize() const;
	unsigned long GetWritableSize() const;

	Region* FindRegion(unsigned long address);
	const Region* FindRegion(unsigned long address) const;

private:
	static unsigned long AlignDown(unsigned long value);
	static unsigned long AlignUp(unsigned long value);

	void ResetLayout();
	void ClearPageInfos();
	void ClearPageTables();
	void ClearPageDirectory();

	bool AddRegion(RegionType type,
				   unsigned long start,
				   unsigned long end,
				   unsigned int prot,
				   unsigned int flags,
				   BackingType backingType);
	void ReservePagesForRegion(unsigned int regionIndex);
	bool AllocateZeroedPage(unsigned long virtualAddress);
	bool ShareTextPage(unsigned long virtualAddress, unsigned long textPhysicalAddress);
	void FreePageInfo(PageInfo& pageInfo, bool releaseSharedText);
	void RemapResidentPages();

	unsigned int AddressToPageIndex(unsigned long address) const;
	void MapPage(unsigned long virtualAddress, unsigned long physicalAddress, bool readWrite);
	void MarkRangeResident(unsigned long virtualAddress,
						   unsigned long size,
						   unsigned long physicalAddress,
						   bool readWrite);
	Region* FindRegionByType(RegionType type);
	const Region* FindRegionByType(RegionType type) const;

private:
	Process* m_Owner;
	PageDirectory* m_PageDirectory;
	PageTable* m_UserPageTableArray;
	Region* m_Regions;
	PageInfo* m_PageInfos;
	bool m_UsesKernelAddressSpace;

	unsigned int m_RegionCount;

	unsigned long m_EntryPoint;
	unsigned long m_HeapBase;
	unsigned long m_HeapBreak;
	unsigned long m_StackTop;
};

#endif
