#include "PEParser.h"
#include "Utility.h"
#include "PageManager.h"
#include "MemoryDescriptor.h"
#include "User.h"
#include "Kernel.h"
#include "Video.h"

namespace
{
	static bool ReadInPagedChunks(Inode* p_inode,
		unsigned long fileOffset,
		unsigned long virtualAddress,
		unsigned long size)
	{
		User& u = Kernel::Instance().GetUser();
		unsigned long remaining = size;

		while ( remaining != 0 )
		{
			unsigned long pageRemain =
				PageManager::PAGE_SIZE - (virtualAddress & (PageManager::PAGE_SIZE - 1));
			unsigned long chunk = remaining;
			if ( chunk > pageRemain )
			{
				chunk = pageRemain;
			}

			u.u_IOParam.m_Base = (unsigned char*)virtualAddress;
			u.u_IOParam.m_Offset = fileOffset;
			u.u_IOParam.m_Count = chunk;
			p_inode->ReadI();

			if ( u.u_error != User::NOERROR || u.u_IOParam.m_Count != 0 )
			{
				if ( u.u_error == User::NOERROR )
				{
					u.u_error = User::ENOEXEC;
				}
				return false;
			}

			fileOffset += chunk;
			virtualAddress += chunk;
			remaining -= chunk;
		}

		return true;
	}

	static void CopyInPagedChunks(unsigned long srcAddress,
		unsigned long desAddress,
		unsigned long size)
	{
		unsigned long remaining = size;

		while ( remaining != 0 )
		{
			unsigned long pageRemain =
				PageManager::PAGE_SIZE - (desAddress & (PageManager::PAGE_SIZE - 1));
			unsigned long chunk = remaining;
			if ( chunk > pageRemain )
			{
				chunk = pageRemain;
			}

			Utility::MemCopy(srcAddress, desAddress, chunk);
			srcAddress += chunk;
			desAddress += chunk;
			remaining -= chunk;
		}
	}
}

PEParser::PEParser()
{
    this->EntryPointAddress = 0;
    this->sectionHeaders = 0;
}

/* 原来V6++的PEParser */
PEParser::PEParser(unsigned long peAddress)
{
	this->peAddress = peAddress + 0xC0000000;   // pe头的虚地址
}

unsigned int PEParser::Relocate(Inode* p_inode, int sharedText)
{
	User& u = Kernel::Instance().GetUser();
	unsigned long srcAddress, desAddress;
	unsigned cnt = 0;
	unsigned int i = 0;
	unsigned int lastDataSectionIdx = this->BSS_SECTION_IDX;

	if ( this->ntHeader.FileHeader.NumberOfSections > this->IDATA_SECTION_IDX )
	{
		lastDataSectionIdx = this->IDATA_SECTION_IDX;
	}

	/* 如果可以和其它进程共享正文段，无需文件中读入正文段 */
	unsigned int textBegin = this->TextAddress >> 12;
	unsigned int textLength =
		(this->TextSize + PageManager::PAGE_SIZE - 1) >> 12;

	/*如果与其它进程共享正文段，共享正文段切不可清0*/
	if(sharedText == 1)
		i = 1;      // i是段头索引
	else
	{
		i = 0;
		// 修改正文段的读写标志，为内核写代码段做准备
		for (unsigned int page = textBegin; page < textBegin + textLength; ++page)
		{
			unsigned int tableIndex = page / PageTable::ENTRY_CNT_PER_PAGETABLE;
			unsigned int entryIndex = page % PageTable::ENTRY_CNT_PER_PAGETABLE;
			PageTable* pageTable = u.u_procp->p_memory.GetUserPageTableByIndex(tableIndex);
			if ( pageTable == NULL )
			{
				continue;
			}

			pageTable->m_Entrys[entryIndex].m_ReadWriter = 1;
		}
		X86Assembly::FlushPageDirectory((unsigned long)u.u_procp->p_memory.GetPageDirectoryPointer());
	}

    /* 对所有页面执行清0操作，这样bss变量的初值就是0 */
	for (; i <= lastDataSectionIdx; i++ )
	{
		ImageSectionHeader* sectionHeader = &(this->sectionHeaders[i]);
		unsigned long beginVM =
			sectionHeader->VirtualAddress + ntHeader.OptionalHeader.ImageBase;
		unsigned long size =
			((sectionHeader->Misc.VirtualSize + PageManager::PAGE_SIZE - 1) >> 12) << 12;

		for (unsigned long j = 0; j < size; ++j)
		{
			unsigned char* b = (unsigned char*)(beginVM + j);
			*b = 0;
		}
	}

	Diagnose::Write("Section initialize finished. i=%d\n",i);

	/* 读正文段（optional）；读文件，得全局变量的初值  */
	if ( sharedText == 1 )
	{
		i = 1;      // i是段头索引
	}
	else
	{
		// 修改正文段的读写标志，为内核写代码段做准备
		i = 0;
	}

	for ( ; i <= lastDataSectionIdx; i++ )
	{
		if ( i == this->BSS_SECTION_IDX )
		{
			continue;
		}

		ImageSectionHeader* sectionHeader = &(this->sectionHeaders[i]);
		srcAddress = sectionHeader->PointerToRawData;
		desAddress =
			this->ntHeader.OptionalHeader.ImageBase + sectionHeader->VirtualAddress;
		if ( ReadInPagedChunks(p_inode,
			srcAddress,
			desAddress,
			sectionHeader->Misc.VirtualSize) == false )
		{
			delete [] this->sectionHeaders;
			this->sectionHeaders = 0;
			return cnt;
		}

		cnt += sectionHeader->Misc.VirtualSize;
	}

	if(sharedText == 0)
	{   //将正文段页面改回只读
		for (unsigned int page = textBegin; page < textBegin + textLength; ++page)
		{
			unsigned int tableIndex = page / PageTable::ENTRY_CNT_PER_PAGETABLE;
			unsigned int entryIndex = page % PageTable::ENTRY_CNT_PER_PAGETABLE;
			PageTable* pageTable = u.u_procp->p_memory.GetUserPageTableByIndex(tableIndex);
			if ( pageTable == NULL )
			{
				continue;
			}

			pageTable->m_Entrys[entryIndex].m_ReadWriter = 0;
		}
		X86Assembly::FlushPageDirectory((unsigned long)u.u_procp->p_memory.GetPageDirectoryPointer());
	}

	delete [] this->sectionHeaders;
	this->sectionHeaders = 0;
	return 	cnt;
}

/* 原来V6++使用的代码，现废弃不用了 */
unsigned int PEParser::Relocate()
{
	unsigned long srcAddress, desAddress;
	unsigned cnt = 0;

	for (unsigned int i = 0; i < this->BSS_SECTION_IDX; i++ )
	{
		ImageSectionHeader* sectionHeader = &(this->sectionHeaders[i]);
		srcAddress = this->peAddress + sectionHeader->PointerToRawData;
		desAddress = 
			this->ntHeader.OptionalHeader.ImageBase + sectionHeader->VirtualAddress;
		CopyInPagedChunks(srcAddress, desAddress, sectionHeader->Misc.VirtualSize);
		cnt += sectionHeader->Misc.VirtualSize;
	}

	return 	cnt;
}

bool PEParser::HeaderLoad(Inode* p_inode)
{
    ImageDosHeader dos_header;
    User& u = Kernel::Instance().GetUser();

    /*读取dos header*/
    u.u_IOParam.m_Base = (unsigned char*)&dos_header;
    u.u_IOParam.m_Offset = 0;
    u.u_IOParam.m_Count = 0x40;
    p_inode->ReadI();       //文件IO不会因为多次ReadI而增加。有缓存的！

    /*读取nt_Header*/
    //ntHeader = (ImageNTHeader*)(kpm.AllocMemory(ntHeader_size)+0xC0000000);
    u.u_IOParam.m_Base = (unsigned char*)(&this->ntHeader);
    u.u_IOParam.m_Offset = dos_header.e_lfanew;
    u.u_IOParam.m_Count = ntHeader_size;
    p_inode->ReadI();

    if ( ntHeader.Signature!=0x00004550 )
	{
		//kpm.FreeMemory(ntHeader_size, (unsigned long)ntHeader - 0xC0000000 );
        return false;
	}


    /* 原本V6++内核 ：读取Section tables至页表区。这是无奈之举，核心态用不了malloc！！
     * 希望内核用  new 和 free 函数申请动态数组。但现在的new操作符好像不对。先这么着。
     * sectionHeaders = new ImageSectionHeader;
     * */
	if ( this->sectionHeaders != 0 )
	{
		delete [] this->sectionHeaders;
		this->sectionHeaders = 0;
	}

	this->sectionHeaders =
		new ImageSectionHeader[this->ntHeader.FileHeader.NumberOfSections];
	if ( this->sectionHeaders == 0 )
	{
		return false;
	}
    u.u_IOParam.m_Base = (unsigned char*)sectionHeaders;
    u.u_IOParam.m_Offset = dos_header.e_lfanew + ntHeader_size;
    u.u_IOParam.m_Count = section_size * ntHeader.FileHeader.NumberOfSections;
    p_inode->ReadI();

    /*
    	 * @comment 这里hardcode gcc的逻辑
    	 * section 顺序为 .text->.data->.rdata->.bss
    	 *
    */
	this->TextAddress =
		this->sectionHeaders[this->TEXT_SECTION_IDX].VirtualAddress +
		ntHeader.OptionalHeader.ImageBase;
	this->TextSize =
		this->sectionHeaders[this->TEXT_SECTION_IDX].Misc.VirtualSize;
	if ( this->TextSize < this->sectionHeaders[this->TEXT_SECTION_IDX].SizeOfRawData )
	{
		this->TextSize = this->sectionHeaders[this->TEXT_SECTION_IDX].SizeOfRawData;
	}

	this->DataAddress =
		ntHeader.OptionalHeader.BaseOfData + ntHeader.OptionalHeader.ImageBase;
	unsigned long dataEnd =
		this->sectionHeaders[this->BSS_SECTION_IDX].VirtualAddress +
		this->sectionHeaders[this->BSS_SECTION_IDX].Misc.VirtualSize;
	if ( ntHeader.FileHeader.NumberOfSections > this->IDATA_SECTION_IDX )
	{
		unsigned long idataSize = this->sectionHeaders[this->IDATA_SECTION_IDX].Misc.VirtualSize;
		if ( idataSize < this->sectionHeaders[this->IDATA_SECTION_IDX].SizeOfRawData )
		{
			idataSize = this->sectionHeaders[this->IDATA_SECTION_IDX].SizeOfRawData;
		}
		unsigned long idataEnd =
			this->sectionHeaders[this->IDATA_SECTION_IDX].VirtualAddress + idataSize;
		if ( idataEnd > dataEnd )
		{
			dataEnd = idataEnd;
		}
	}
	this->DataSize = dataEnd - ntHeader.OptionalHeader.BaseOfData;

    StackSize = ntHeader.OptionalHeader.SizeOfStackCommit;
    HeapSize = ntHeader.OptionalHeader.SizeOfHeapCommit;

    EntryPointAddress = ntHeader.OptionalHeader.AddressOfEntryPoint +
                    ntHeader.OptionalHeader.ImageBase;

	return true;
}
