#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "List.h"

class MemoryDescriptor;

class PageManager
{
public:
	/* static member */
	static unsigned int PHY_MEM_SIZE;	/* 物理内存大小，系统启动时根据物理内存大小设置 */
	
	/* static const member */
	static const unsigned int PAGE_SIZE = 0x1000;					/* 物理内存页大小 */
	static const unsigned int MAX_BITMAP_PAGE_COUNT = 0x1c00;	/* bitmap 最多跟踪 7168 页，匹配当前 32MB 配置 */
	static const unsigned int BITMAP_WORD_BITS = sizeof(unsigned long) * 8;
	static const unsigned int BITMAP_WORD_COUNT =
		(MAX_BITMAP_PAGE_COUNT + BITMAP_WORD_BITS - 1) / BITMAP_WORD_BITS;
	static const unsigned int KERNEL_MEM_START_ADDR	= 0x100000;		/* 内核映像从1M物理内存开始 */
	static const unsigned int KERNEL_SIZE			= 0x80000;		/* 内核映像大小限制(一般二进制映像远不会到512K大小) */

	/* Functions */
public:
	PageManager();
	virtual ~PageManager();
	
	/* 初始化并清空 bitmap 状态 */
	int Initialize();
	/* 按页分配，返回物理地址，失败返回0 */
	unsigned long AllocatePages(unsigned long pageCount);
	/* 按页释放，startAddress 需页对齐 */
	unsigned long FreePages(unsigned long pageCount, unsigned long startAddress);
	unsigned long AllocatePage();
	unsigned long FreePage(unsigned long startAddress);
	unsigned long GetFreePageCount() const;
	unsigned long GetTotalPageCount() const;

protected:
	int InitializePool(unsigned long poolStartAddress, unsigned long poolSizeBytes);

	/* Members */
private:
	bool IsPageUsed(unsigned long pageIndex) const;
	void MarkPagesUsed(unsigned long startPage, unsigned long pageCount);
	void MarkPagesFree(unsigned long startPage, unsigned long pageCount);

private:
	unsigned long m_Bitmap[PageManager::BITMAP_WORD_COUNT];
	unsigned long m_PoolStartAddress;
	unsigned long m_TotalPageCount;
};


class KernelPageManager : public PageManager
{
public:
	/* 
	 * 物理地址 0x200000 被用于共享核心页表，
	 * 物理地址 0x201000 被用于共享 0# 用户页表，
	 * 物理地址 0x202000 被用于 0# 进程页目录，
	 * 0x203000 起用于普通进程的页目录和私有 1# 用户页表。
	 */
	static const unsigned int KERNEL_PAGE_POOL_START_ADDR = 0x203000;
	static const unsigned int KERNEL_PAGE_POOL_SIZE = 0x200000 - 0x3000;

public:
	KernelPageManager();
	int Initialize();	/* 初始化内核物理页池 bitmap */
};


class UserPageManager : public PageManager
{
public:
	struct ReverseMapEntry
	{
		ListHead frameNode;
		MemoryDescriptor* owner;
		unsigned short virtualPageIndex;
		int debugPid;
	};

	struct FrameInfo
	{
		ListHead rmapHead;
		unsigned short mapCount;
		unsigned short flags;
		unsigned short cowRefCount;
		unsigned int clockAge;
	};

	/* static const member */
	static const unsigned int USER_PAGE_POOL_START_ADDR = 0x400000;		/* 用户物理内存区域起始地址 */
	static const unsigned int USER_ZERO_PAGE_ADDRESS = USER_PAGE_POOL_START_ADDR;	/* 固定零页，供 BACKING_ZERO 共享映射 */
	static const unsigned short FRAME_FLAG_NONE = 0x0;
	static const unsigned short FRAME_FLAG_ZERO_PAGE = 0x1;
	static const unsigned short FRAME_FLAG_COW = 0x2;
	static const unsigned short FRAME_FLAG_SHARED_TEXT = 0x4;
	static const unsigned short FRAME_FLAG_PINNED = 0x8;

	static const unsigned int RECLAIM_HIGH_WATERMARK_PERCENT = 80;
	static const unsigned int RECLAIM_LOW_WATERMARK_PERCENT = 70;

	/* static member */
	static unsigned int USER_PAGE_POOL_SIZE;		/* 用户物理内存区域大小：由内核初始化时进行设置 */

public:
	UserPageManager();
	int Initialize();	/* 初始化用户物理页池 bitmap */
	unsigned long AllocatePages(unsigned long pageCount);
	unsigned long FreePages(unsigned long pageCount, unsigned long startAddress);
	unsigned long AllocatePage();
	unsigned long FreePage(unsigned long startAddress);
	unsigned long GetZeroPageAddress() const;
	bool IsZeroPage(unsigned long pageAddress) const;
	bool ResolvePoolPageIndex(unsigned long pageAddress, unsigned long& pageIndex) const;
	FrameInfo* GetFrameInfoByAddress(unsigned long pageAddress);
	const FrameInfo* GetFrameInfoByAddress(unsigned long pageAddress) const;
	unsigned long GetPageAddressByIndex(unsigned long pageIndex) const;
	bool AttachReverseMap(unsigned long pageAddress,
		ReverseMapEntry* entry,
		unsigned short frameFlags);
	void DetachReverseMap(unsigned long pageAddress, ReverseMapEntry* entry);
	unsigned short GetFrameMapCount(unsigned long pageAddress) const;
	unsigned short GetFrameFlags(unsigned long pageAddress) const;
	void SetFrameFlags(unsigned long pageAddress, unsigned short flags);
	bool ReclaimUntilLowWatermark();

	/* 将页纳入 COW 追踪：首次共享时置为2，后续共享继续递增。 */
	bool ShareAsCopyOnWrite(unsigned long pageAddress);
	/* 查询页的 COW 引用计数，0 表示未追踪。 */
	unsigned short GetCopyOnWriteRefCount(unsigned long pageAddress) const;
	/* COW 引用计数减1，返回新计数。 */
	unsigned short ReleaseCopyOnWriteRef(unsigned long pageAddress);
	/* 显式设置页的 COW 引用计数，用于从 swap 元数据恢复共享关系。 */
	bool SetCopyOnWriteRefCount(unsigned long pageAddress, unsigned short refCount);
	/* 清除页的 COW 引用计数。 */
	void ClearCopyOnWriteRef(unsigned long pageAddress);

private:
	bool ShouldReclaimBeforeAllocate(unsigned long pageCount) const;
	bool ReclaimOneFrame();
	void ReleaseReclaimPin(unsigned long pageIndex);

private:
	FrameInfo m_FrameInfo[PageManager::MAX_BITMAP_PAGE_COUNT];
	unsigned long m_ClockHand;
	bool m_Reclaiming;
};

#endif
