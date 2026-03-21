#include "MemoryDescriptor.h"
#include "Kernel.h"
#include "PageManager.h"
#include "Machine.h"
#include "PageDirectory.h"
#include "Video.h"

void MemoryDescriptor::Initialize()
{
	KernelPageManager& kernelPageManager = Kernel::Instance().GetKernelPageManager();
	
	/* m_UserPageTableArray需要把AllocMemory()返回的物理内存地址 + 0xC0000000 */
	this->m_UserPageTableArray = (PageTable*)(kernelPageManager.AllocMemory(sizeof(PageTable) * USER_SPACE_PAGE_TABLE_CNT) + Machine::KERNEL_SPACE_START_ADDRESS);
}

void MemoryDescriptor::Release()
{
	KernelPageManager& kernelPageManager = Kernel::Instance().GetKernelPageManager();
	if ( this->m_UserPageTableArray )
	{
		kernelPageManager.FreeMemory(sizeof(PageTable) * USER_SPACE_PAGE_TABLE_CNT, (unsigned long)this->m_UserPageTableArray - Machine::KERNEL_SPACE_START_ADDRESS);
		this->m_UserPageTableArray = NULL;
	}
}

unsigned int MemoryDescriptor::MapEntry(unsigned long virtualAddress, unsigned int size, unsigned long phyPageIdx, bool isReadWrite)
{	
	unsigned long address = virtualAddress - USER_SPACE_START_ADDRESS;
	
	//计算从pagetable的哪一个地址开始映射
	unsigned long startIdx = address >> 12;
	unsigned long cnt = ( size + (PageManager::PAGE_SIZE - 1) )/ PageManager::PAGE_SIZE;

	PageTableEntry* entrys = (PageTableEntry*)this->m_UserPageTableArray;
	for ( unsigned int i = startIdx; i < startIdx + cnt; i++, phyPageIdx++ )
	{
		entrys[i].m_Present = 0x1;
		entrys[i].m_UserSupervisor = 0x1;
		entrys[i].m_ReadWriter = isReadWrite;
		entrys[i].m_PageBaseAddress = phyPageIdx;
	}
	return phyPageIdx;
}

void MemoryDescriptor::MapTextEntrys(unsigned long textStartAddress, unsigned long textSize, unsigned long textPageIdx)
{
	this->MapEntry(textStartAddress, textSize, textPageIdx, false);
}
void MemoryDescriptor::MapDataEntrys(unsigned long dataStartAddress, unsigned long dataSize, unsigned long dataPageIdx)
{
	this->MapEntry(dataStartAddress, dataSize, dataPageIdx, true);
}

void MemoryDescriptor::MapStackEntrys(unsigned long stackSize, unsigned long stackPageIdx)
{
	unsigned long stackStartAddress = (USER_SPACE_START_ADDRESS + USER_SPACE_SIZE - stackSize) & 0xFFFFF000;
	this->MapEntry(stackStartAddress, stackSize, stackPageIdx, true);
}

PageTable* MemoryDescriptor::GetUserPageTableArray()
{
	return this->m_UserPageTableArray;
}
unsigned long MemoryDescriptor::GetTextStartAddress()
{
	return this->m_TextStartAddress;
}
unsigned long MemoryDescriptor::GetTextSize()
{
	return this->m_TextSize;
}
unsigned long MemoryDescriptor::GetDataStartAddress()
{
	return this->m_DataStartAddress;
}
unsigned long MemoryDescriptor::GetDataSize()
{
	return this->m_DataSize;
}
unsigned long MemoryDescriptor::GetStackSize()
{
	return this->m_StackSize;
}

bool MemoryDescriptor::CheckUserSpaceSize( unsigned long textVirtualAddress, unsigned long textSize, unsigned long dataVirtualAddress, unsigned long dataSize, unsigned long stackSize )
{
	User& u = Kernel::Instance().GetUser();

	/* 如果超出允许的用户程序最大8M的地址空间限制 */
	if ( textSize + dataSize + stackSize  + PageManager::PAGE_SIZE > USER_SPACE_SIZE - textVirtualAddress)
	{
		u.u_error = User::ENOMEM;
		Diagnose::Write("u.u_error = %d\n",u.u_error);
		return false;
	}
}

/* 因为需要供Newproc调用，所以只写页表，不建立地址映射关系 */
void MemoryDescriptor::EstablishUserPageTable( unsigned long textVirtualAddress, unsigned long textSize, unsigned long dataVirtualAddress, unsigned long dataSize, unsigned long stackSize )
{
	User& u = Kernel::Instance().GetUser();

	// this->ClearUserPageTable();

	/* 以相对起始地址phyPageIndex为0，为正文段建立页表 */
	unsigned int phyPageIndex = 0 + u.u_procp->p_textp->x_caddr>>12;
	phyPageIndex = this->MapEntry(textVirtualAddress, textSize, phyPageIndex, false);

	/* 以相对起始地址phyPageIndex为1，ppda区占用1页4K大小物理内存，为数据段建立页表 */
	phyPageIndex = 1 + (u.u_procp->p_addr>>12);
	phyPageIndex = this->MapEntry(dataVirtualAddress, dataSize, phyPageIndex, true);

	/* 紧跟着数据段之后，为堆栈段建立页表 */
	unsigned long stackStartAddress = (USER_SPACE_START_ADDRESS + USER_SPACE_SIZE - stackSize) & 0xFFFFF000;
	this->MapEntry(stackStartAddress, stackSize, phyPageIndex, true);

	this->MapEntry(0, 4096, 0, true);  /* 为0#逻辑页建立页表项，将其映射至0#物理页框 */

	// this->MapToPageTable();
// 	return true;
}

void MemoryDescriptor::DisplayPageTable()
{
	unsigned int i,j;

	Diagnose::Write("Process PT:");
	for (i = 0; i < Machine::USER_PAGE_TABLE_CNT; i++)
		for ( j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; j++)
			if ( 1 == this->m_UserPageTableArray[i].m_Entrys[j].m_Present )
				Diagnose::Write("<%d,%x>  ",i*1024+j,this->m_UserPageTableArray[i].m_Entrys[j].m_PageBaseAddress);
	Diagnose::Write("\n");

	Diagnose::Write("<PPDA,%x>  ",Machine::Instance().GetKernelPageTable().m_Entrys[1023].m_PageBaseAddress);

	PageTable* pUserPageTable = (PageTable*)((unsigned int)(Machine::Instance().GetPageDirectory().m_Entrys[0].m_PageTableBaseAddress) << 12 | 0xC0000000);
	Diagnose::Write("User PT: %x", (unsigned int)pUserPageTable);

//	for (i = 0; i < Machine::USER_PAGE_TABLE_CNT; i++)
		for ( j = 1; j < PageTable::ENTRY_CNT_PER_PAGETABLE; j++)
			if ( 1 == pUserPageTable[1].m_Entrys[j].m_Present )
				Diagnose::Write("<%d,%x>  ",1*1024+j,pUserPageTable[1].m_Entrys[j].m_PageBaseAddress);
	Diagnose::Write("\n");
}

void MemoryDescriptor::ClearUserPageTable()
{
	User& u = Kernel::Instance().GetUser();
	PageTable* pUserPageTable = u.u_MemoryDescriptor.m_UserPageTableArray;

	unsigned int i ;
	unsigned int j ;

	for (i = 0; i < Machine::USER_PAGE_TABLE_CNT; i++)
	{
		for (j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; j++ )
		{
			pUserPageTable[i].m_Entrys[j].m_Present = 0;
			pUserPageTable[i].m_Entrys[j].m_ReadWriter = 0;
			pUserPageTable[i].m_Entrys[j].m_UserSupervisor = 1;
			pUserPageTable[i].m_Entrys[j].m_PageBaseAddress = 0;
		}
	}

}

/*
void MemoryDescriptor::MapToPageTable()
{
	User& u = Kernel::Instance().GetUser();

	if(u.u_MemoryDescriptor.m_UserPageTableArray == NULL)
		return;

	PageTable* pUserPageTable = Machine::Instance().GetUserPageTableArray();
	unsigned int textAddress = 0;
	if ( u.u_procp->p_textp != NULL )
	{
		textAddress = u.u_procp->p_textp->x_caddr;
	}

	for (unsigned int i = 0; i < Machine::USER_PAGE_TABLE_CNT; i++)
	{
		for ( unsigned int j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; j++ )
		{
			pUserPageTable[i].m_Entrys[j].m_Present = 0;   //先清0

			if ( 1 == this->m_UserPageTableArray[i].m_Entrys[j].m_Present )
			{
				pUserPageTable[i].m_Entrys[j].m_Present = 1;
				pUserPageTable[i].m_Entrys[j].m_ReadWriter = this->m_UserPageTableArray[i].m_Entrys[j].m_ReadWriter;
				pUserPageTable[i].m_Entrys[j].m_PageBaseAddress = this->m_UserPageTableArray[i].m_Entrys[j].m_PageBaseAddress;
			}
		}
	}

	FlushPageDirectory();
}*/

void MemoryDescriptor::MapToPageTable()
{
	User& u = Kernel::Instance().GetUser();
	Machine& machine = Machine::Instance();

    unsigned long phyFrame = (unsigned long)(u.u_MemoryDescriptor.m_UserPageTableArray);  // 相对表（现在已经是页表了）首地址，虚地址

	if(phyFrame == NULL)
		return;
	else
		phyFrame = (phyFrame - 0xC0000000) >> 12;   // 相对表（现在已经是页表了）物理页框号

	Diagnose::Write("Start Address of Process's User Page Table: %x，%x\n",(unsigned long)(u.u_MemoryDescriptor.m_UserPageTableArray),phyFrame);

	for ( unsigned int i = 0; i < Machine::USER_PAGE_TABLE_CNT; i++, phyFrame++ )
	{
		Machine::Instance().GetPageDirectory().m_Entrys[i].m_UserSupervisor = 1;
		Machine::Instance().GetPageDirectory().m_Entrys[i].m_Present = 1;
		Machine::Instance().GetPageDirectory().m_Entrys[i].m_ReadWriter = 1;
		Machine::Instance().GetPageDirectory().m_Entrys[i].m_PageTableBaseAddress = phyFrame;
	}

	FlushPageDirectory();
	PageDirectory * pPageDirectory = & machine.GetPageDirectory();
	Diagnose::Write("PageTable used by CPU %x\n", Machine::Instance().GetPageDirectory().m_Entrys[0].m_PageTableBaseAddress);
}
