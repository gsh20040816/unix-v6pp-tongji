#ifndef MEMORY_DESCRIPTOR_H
#define MEMORY_DESCRIPTOR_H

#include "PageDirectory.h"
#include "PageTable.h"
#include "INode.h"
#include "Text.h"
#include "PageManager.h"
#include "Vector.h"

class Process;

/*
 * MemoryDescriptor 描述单个进程的用户态虚拟内存布局与驻留页状态。
 * 它负责：
 * 1) 维护区域布局（code/data/heap/stack 等）；
 * 2) 管理页状态（FREE/RESERVED/RESIDENT）；
 * 3) 在缺页时按 BackingType 将页映射到物理内存。
 */
class MemoryDescriptor
{
public:
	/* 用户地址空间起始虚拟地址（含）。 */
	static const unsigned long USER_SPACE_START = 0x00000000;
	/* 用户地址空间结束虚拟地址（不含）。 */
	static const unsigned long USER_SPACE_END = 0x00800000;
	/* 用户地址空间总大小。 */
	static const unsigned long USER_SPACE_SIZE = USER_SPACE_END - USER_SPACE_START;
	/* 用户空间覆盖的页表数量（0# + 1#）。 */
	static const unsigned int USER_PAGE_TABLE_CNT = 0x2;
	/* 进程私有用户页表数量（当前仅 1# 私有）。 */
	static const unsigned int USER_PRIVATE_PAGE_TABLE_CNT = 0x1;
	/* 用户空间总页数。 */
	static const unsigned int USER_PAGE_COUNT = USER_SPACE_SIZE / PageManager::PAGE_SIZE;
	/* 最多支持的虚拟内存区域数量。 */
	static const unsigned int MAX_REGION_COUNT = 16;

	/* 用户空间逻辑区域类型。 */
	enum RegionType
	{
		/* 无效区域。 */
		REGION_INVALID = 0,
		/* 低地址运行时页（如 0 页运行时约定）。 */
		REGION_RUNTIME,
		/* 可执行代码段。 */
		REGION_CODE,
		/* 只读数据段（当前布局中可选）。 */
		REGION_RODATA,
		/* 已初始化可写数据段。 */
		REGION_DATA,
		/* 未初始化数据段。 */
		REGION_BSS,
		/* 堆区（brk 管理）。 */
		REGION_HEAP,
		/* 用户栈区（向低地址增长）。 */
		REGION_STACK
	};

	/* 页的后备来源类型，决定缺页时如何补页。 */
	enum BackingType
	{
		/* 无后备。 */
		BACKING_NONE = 0,
		/* 以全零页作为后备。 */
		BACKING_ZERO,
		/* 来自可执行文件内容（按需缺页回填）。 */
		BACKING_EXEC_FILE,
		/* 共享 Text 物理页。 */
		BACKING_SHARED_TEXT,
		/* 匿名页后备（如 runtime 区）。 */
		BACKING_ANON
	};

	/* 单页在 MemoryDescriptor 视角下的生命周期状态。 */
	enum PageState
	{
		/* 未被任何区域占用。 */
		PAGE_STATE_FREE = 0,
		/* 已归属某区域，但未映射到物理页。 */
		PAGE_STATE_RESERVED,
		/* 已映射到物理页并可访问。 */
		PAGE_STATE_RESIDENT,
		/* 已换出到 swap，当前无驻留物理页。 */
		PAGE_STATE_SWAPPED
	};

	/* 区域访问权限位。 */
	enum ProtFlags
	{
		/* 可读。 */
		PROT_READ = 0x1,
		/* 可写。 */
		PROT_WRITE = 0x2,
		/* 可执行。 */
		PROT_EXEC = 0x4,
		/* 用户态可访问。 */
		PROT_USER = 0x8
	};

	/* 区域/页附加属性位。 */
	enum PageFlags
	{
		/* 无额外标记。 */
		PAGE_FLAG_NONE = 0x0,
		/* 脏页标记。 */
		PAGE_FLAG_DIRTY = 0x1,
		/* 访问标记。 */
		PAGE_FLAG_ACCESSED = 0x2,
		/* 可向低地址方向增长（栈）。 */
		PAGE_FLAG_GROWSDOWN = 0x4,
		/* 换出前属于 COW 共享页，换入后继续只读共享。 */
		PAGE_FLAG_SWAPPED_COW = 0x8
	};

	/* 区域的后备信息。 */
	struct BackingStore
	{
		/* 后备类型。 */
		BackingType type;
		/* 文件后备对应 inode（如 BACKING_EXEC_FILE）。 */
		Inode* inode;
		/* 共享正文后备对应 Text 结构。 */
		Text* text;
		/* 在后备对象中的起始偏移。 */
		unsigned long fileOffset;
		/* 该区域有效字节数。 */
		unsigned long validBytes;
	};

	/* 每个用户虚拟页的元数据。 */
	struct PageInfo
	{
		/* 页状态。 */
		PageState state;
		/* 页标记（PageFlags）。 */
		unsigned int flags;
		/* 所属区域索引，0xffff 表示无效。 */
		unsigned short regionIndex;
		/* 当前映射的物理页帧地址（RESIDENT 时有效）。 */
		unsigned long frameAddress;
		/* 相对后备窗口起点的偏移，用于按需回填。 */
		unsigned long backingOffset;
		/* 当前驻留映射在物理页反向映射链中的结点，生命周期跟随 PageInfo。 */
		UserPageManager::ReverseMapEntry rmap;
		/* rmap 当前是否挂在某个物理页的反向映射链上。 */
		bool rmapAttached;
		/* PAGE_STATE_SWAPPED 时记录 swap slot。 */
		unsigned int swapSlot;
	};

	/* 连续虚拟地址区域描述。 */
	struct Region
	{
		/* 区域起始地址（含）。 */
		unsigned long start;
		/* 区域结束地址（不含）。 */
		unsigned long end;
		/* 区域权限组合（ProtFlags）。 */
		unsigned int prot;
		/* 区域附加属性（PageFlags）。 */
		unsigned int flags;
		/* 区域类型。 */
		RegionType type;
		/* 区域后备信息。 */
		BackingStore backing;
		/*
		 * 固定大小区域直接用定长数组，避免额外容量管理；
		 * heap/stack 这类单向增长区域改用 Vector，扩展时只改本区域元数据。
		 */
		PageInfo* fixedPageInfos;
		Vector<PageInfo> dynamicPageInfos;
	};

	/* 导出用户驻留页信息时使用的轻量快照结构。 */
	struct UserPageSnapshotEntry
	{
		/* 页索引（相对 USER_SPACE_START）。 */
		unsigned short pageIndex;
		/* 页表位快照（present/rw/us/exec）。 */
		unsigned short flags;
		/* 页帧号（页基址）。 */
		unsigned int pageBaseAddress;
	};

public:
	/* 构造函数：初始化成员并清空布局状态。 */
	MemoryDescriptor();
	/* 析构函数：资源释放由 Release/Reset 路径控制。 */
	~MemoryDescriptor();

	/* 绑定所属进程，供缺页/共享正文等路径访问进程状态。 */
	void Attach(Process* owner);
	/* 清空布局与页状态；不释放已分配元数据容器。 */
	void Reset();
	/* 分配并初始化页目录、私有页表、区域数组、页信息数组。 */
	void Initialize();
	/* 释放 MemoryDescriptor 自有资源。 */
	void Release();

	/* 切换为仅引用内核地址空间（不拥有私有页表和元数据）。 */
	void UseKernelAddressSpace(PageDirectory* pageDirectory);
	/* 拷贝布局与保留态页元数据（不复制物理页内容）。 */
	void CloneFrom(const MemoryDescriptor& other);
	/* 将 other 的驻留页复制/共享到当前地址空间。 */
	bool CloneResidentPagesFrom(const MemoryDescriptor& other);

	/* 根据可执行文件信息建立 code/rodata/data/heap/stack 区域布局。 */
	bool ConfigureExecutableLayout(unsigned long entryPoint,
								   unsigned long codeStart,
								   unsigned long codeSize,
								   unsigned long dataStart,
								   unsigned long dataSize,
								   unsigned long rodataStart,
								   unsigned long rodataSize,
								   unsigned long bssStart,
								   unsigned long bssSize,
								   unsigned long stackSize);
	/* 为 BACKING_EXEC_FILE 区域配置文件后备窗口（exec 后调用一次）。 */
	void ConfigureExecFileBacking(unsigned long virtualBase,
		unsigned long fileOffset,
		unsigned long fileSize,
		Inode* inode);

	/* 校验区域边界和相互重叠关系是否合法。 */
	bool CheckUserSpace() const;
	/* 处理用户地址页故障（缺页/写保护），必要时扩栈并触发按需补页。 */
	bool HandlePageFault(unsigned long faultAddress,
		unsigned long stackPointer,
		bool isUserMode,
		unsigned int errorCode);
	/* 初始化早期引导所需的最小用户布局和栈页。 */
	bool MaterializeBootstrapStack();
	/* 为 exec 后的新映像做必要驻留：当前仅预分配用户栈。 */
	bool MaterializeExecutableImage();
	/* 将已分配的用户物理页挂接到指定虚拟页，供 exec 等路径复用。 */
	bool InstallResidentPage(unsigned long virtualAddress, unsigned long physicalAddress);
	/* 确保 faultAddress 所在页进入 RESIDENT 并完成页表映射。 */
	bool EnsurePagePresent(unsigned long faultAddress);
	/* 释放当前所有驻留页；可选择是否释放共享正文页。 */
	void ReleaseResidentPages(bool releaseSharedText);

	/* 清空并按 PageInfo 重新构建用户页表映射。 */
	void BuildPageTablesForImage();
	/* 将当前页目录/页表写入机器状态，使其生效。 */
	void Activate();
	/* 打印当前进程用户页表中的已映射项。 */
	void DisplayPageTable();
	/* 导出驻留页快照，返回驻留页总数。 */
	unsigned int ExportResidentUserPages(UserPageSnapshotEntry* entries,
		unsigned int maxEntries) const;

	/* 调整堆顶（brk）：支持收缩和扩展，按页更新保留态。 */
	bool SetHeapBreak(unsigned long newBreak);
	/* 将栈底向下扩展一页，并把新页标记为 RESERVED。 */
	bool GrowStackByPage();

	/* 获取页目录指针。 */
	PageDirectory* GetPageDirectoryPointer() const;
	/* 获取私有用户页表数组指针。 */
	PageTable* GetUserPageTableArray() const;
	/* 获取指定用户页表（0# 由 Machine 共享，1# 为进程私有）。 */
	PageTable* GetUserPageTableByIndex(unsigned int index) const;
	/* 当前是否具备用户地址空间（即私有页表是否存在）。 */
	bool HasUserAddressSpace() const;
	/* 供物理页回收器读取候选页的 PTE/后备属性。 */
	bool CollectEvictionInfo(unsigned short virtualPageIndex,
		bool& accessed,
		bool& dirty,
		bool& discardable) const;
	/* 清除候选页 PTE 的 accessed 位，供 clock 回收策略使用。 */
	void ClearPageAccessed(unsigned short virtualPageIndex);
	/* 将驻留页丢弃为原后备 RESERVED 状态。 */
	bool EvictPageToReserved(unsigned short virtualPageIndex);
	/* 将驻留页标记为已换出。 */
	bool EvictPageToSwap(unsigned short virtualPageIndex, unsigned int swapSlot);

	/* 获取入口地址。 */
	unsigned long GetEntryPoint() const;
	/* 获取代码段起始地址。 */
	unsigned long GetCodeStart() const;
	/* 获取代码段大小。 */
	unsigned long GetCodeSize() const;
	/* 获取数据段起始地址。 */
	unsigned long GetDataStart() const;
	/* 获取数据段大小。 */
	unsigned long GetDataSize() const;
	/* 获取堆基址。 */
	unsigned long GetHeapBase() const;
	/* 获取当前堆顶。 */
	unsigned long GetHeapBreak() const;
	/* 获取用户栈顶（高地址端）。 */
	unsigned long GetStackTop() const;
	/* 获取用户栈底（低地址端）。 */
	unsigned long GetStackBottom() const;
	/* 获取栈总大小。 */
	unsigned long GetStackSize() const;
	/* 获取可写用户内存总量（data+heap+stack）。 */
	unsigned long GetWritableSize() const;

	/* 按地址查找所属区域（可写版本）。 */
	Region* FindRegion(unsigned long address);
	/* 按地址查找所属区域（只读版本）。 */
	const Region* FindRegion(unsigned long address) const;

private:
	/* 地址向下按页对齐。 */
	static unsigned long AlignDown(unsigned long value);
	/* 地址向上按页对齐。 */
	static unsigned long AlignUp(unsigned long value);

	/* 重置布局相关成员为初始状态。 */
	void ResetLayout();
	/* 释放全部 Region 持有的 PageInfo 存储。 */
	void ReleaseRegionPageInfos();
	/* 释放单个 Region 的 PageInfo 存储。 */
	void ReleaseRegionPageInfos(Region& region);
	/* 清空所有 PageInfo 项。 */
	void ClearPageInfos();
	/* 将单个 PageInfo 重置为空闲状态。 */
	void ClearPageInfo(PageInfo& pageInfo);
	/* 清空进程私有页表项。 */
	void ClearPageTables();
	/* 清空页目录项（仅自有页目录场景）。 */
	void ClearPageDirectory();

	/* 新增一个区域并登记其后备属性。 */
	bool AddRegion(RegionType type,
				   unsigned long start,
				   unsigned long end,
				   unsigned int prot,
				   unsigned int flags,
				   BackingType backingType);
	/* 将区域覆盖的页全部标记为 RESERVED 并写入 backingOffset。 */
	bool ReservePagesForRegion(unsigned int regionIndex);
	/* 分配一个用户物理页并清零后映射到 virtualAddress。 */
	bool AllocateZeroedPage(unsigned long virtualAddress);
	/* 将 BACKING_ZERO 页只读映射到全局零页，并纳入 COW 追踪。 */
	bool MapZeroPageForCopyOnWrite(unsigned long virtualAddress);
	/* 将共享正文物理页映射到 virtualAddress，可按需设置写权限。 */
	bool ShareTextPage(unsigned long virtualAddress,
		unsigned long textPhysicalAddress,
		bool readWrite);
	/* 处理可写页写保护触发的 COW 分裂。 */
	bool HandleCopyOnWriteFault(unsigned long faultAddress);
	/* 释放单个 PageInfo 的驻留页（按策略可跳过共享正文页）。 */
	void FreePageInfo(PageInfo& pageInfo, bool releaseSharedText);
	/* 按 PageInfo 扫描并恢复所有 RESIDENT 页的页表映射。 */
	void RemapResidentPages();
	/* 固定区域与单向增长区域共用的 PageInfo 访问 helper。 */
	PageInfo* GetPageInfo(Region* region, unsigned int pageOffset);
	const PageInfo* GetPageInfo(const Region* region, unsigned int pageOffset) const;
	/* 按虚拟地址定位所属 Region 和对应 PageInfo。 */
	PageInfo* GetPageInfoByAddress(unsigned long address, Region** region);
	const PageInfo* GetPageInfoByAddress(unsigned long address,
		const Region** region) const;
	/* 计算地址在某个 Region 内的页偏移。 */
	unsigned int GetRegionPageOffset(const Region& region, unsigned long address) const;
	/* 将 Region 内页偏移还原成虚拟页起始地址。 */
	unsigned long GetRegionPageAddress(const Region& region, unsigned int pageOffset) const;
	/* 获取当前 Region 覆盖的页数。 */
	unsigned int GetRegionPageCount(const Region& region) const;
	/* 标记是否为 heap/stack 这类单向增长 Region。 */
	bool IsDynamicPageInfoRegion(RegionType type) const;
	/* 初始化单个 RESERVED PageInfo。 */
	void InitializeReservedPageInfo(PageInfo& pageInfo,
		unsigned int regionIndex,
		unsigned int pageOffset);

	/* 虚拟地址转用户页索引。 */
	unsigned int AddressToPageIndex(unsigned long address) const;
	/* 在用户页表中写入一个映射。 */
	void MapPage(unsigned long virtualAddress, unsigned long physicalAddress, bool readWrite);
	/* 清除指定用户虚页的页表映射。 */
	void ClearPageMapping(unsigned short virtualPageIndex);
	/* 统一建立 PageInfo/PTE/物理页反向映射。 */
	bool AttachFrame(unsigned long virtualAddress,
		unsigned long physicalAddress,
		bool readWrite,
		unsigned short frameFlags);
	/* 统一拆除 PageInfo/PTE/物理页反向映射。 */
	void DetachFrame(PageInfo& pageInfo,
		bool clearPte,
		bool resetToReserved,
		bool freeFrameIfUnmapped);
	/* 按区域类型查找（可写版本）。 */
	Region* FindRegionByType(RegionType type);
	/* 按区域类型查找（只读版本）。 */
	const Region* FindRegionByType(RegionType type) const;

private:
	/* 所属进程。 */
	Process* m_Owner;
	/* 当前进程页目录。 */
	PageDirectory* m_PageDirectory;
	/* 仅保存进程私有的 1# 用户页表，0# 用户页表由 Machine 共享维护。 */
	PageTable* m_UserPageTableArray;
	/* 区域表。 */
	Region* m_Regions;
	/* 是否使用内核地址空间（true 时不拥有私有资源）。 */
	bool m_UsesKernelAddressSpace;

	/* 当前有效区域数量。 */
	unsigned int m_RegionCount;

	/* 进程入口地址。 */
	unsigned long m_EntryPoint;
	/* 堆基址（不可低于此值）。 */
	unsigned long m_HeapBase;
	/* 当前堆顶（brk）。 */
	unsigned long m_HeapBreak;
	/* 栈顶高地址（固定为 USER_SPACE_END）。 */
	unsigned long m_StackTop;
};

#endif
