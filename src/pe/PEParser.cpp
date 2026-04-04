#include "PEParser.h"
#include "Utility.h"
#include "PageManager.h"
#include "User.h"
#include "Kernel.h"

namespace
{
	static unsigned long AlignDownToPage(unsigned long value)
	{
		return value & ~(PageManager::PAGE_SIZE - 1);
	}

	static unsigned long AlignUpToPage(unsigned long value)
	{
		return (value + PageManager::PAGE_SIZE - 1) & ~(PageManager::PAGE_SIZE - 1);
	}

	static void ComputeSharedRodataRange(unsigned long dataAddress,
		unsigned long dataSize,
		unsigned long rodataAddress,
		unsigned long rodataSize,
		unsigned long& sharedStart,
		unsigned long& sharedSize)
	{
		sharedStart = 0;
		sharedSize = 0;

		if ( rodataSize == 0 )
		{
			return;
		}

		unsigned long dataStart = dataAddress;
		unsigned long dataEnd = AlignUpToPage(dataAddress + dataSize);
		unsigned long candidateStart = AlignUpToPage(rodataAddress);
		unsigned long candidateEnd = AlignDownToPage(rodataAddress + rodataSize);

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
			sharedStart = candidateStart;
			sharedSize = candidateEnd - candidateStart;
		}
	}

	static unsigned long ComputeSectionMappedSize(const ImageNTHeader& ntHeader,
		const ImageSectionHeader* sectionHeaders,
		unsigned int sectionCount,
		unsigned int sectionIndex)
	{
		if ( sectionHeaders == NULL || sectionIndex >= sectionCount )
		{
			return 0;
		}

		const ImageSectionHeader& section = sectionHeaders[sectionIndex];
		unsigned long usedSize = section.Misc.VirtualSize;
		if ( usedSize < section.SizeOfRawData )
		{
			usedSize = section.SizeOfRawData;
		}

		if ( usedSize == 0 )
		{
			return 0;
		}

		unsigned long start = section.VirtualAddress + ntHeader.OptionalHeader.ImageBase;
		unsigned long boundary = AlignUpToPage(start + usedSize);
		unsigned long sectionVirtualAddress = section.VirtualAddress;
		unsigned long nextStart = 0xffffffffUL;

		for ( unsigned int i = 0; i < sectionCount; ++i )
		{
			if ( i == sectionIndex )
			{
				continue;
			}

			unsigned long candidateVirtualAddress = sectionHeaders[i].VirtualAddress;
			if ( candidateVirtualAddress <= sectionVirtualAddress )
			{
				continue;
			}

			unsigned long candidateStart = candidateVirtualAddress + ntHeader.OptionalHeader.ImageBase;
			if ( candidateStart < nextStart )
			{
				nextStart = candidateStart;
			}
		}

		if ( nextStart != 0xffffffffUL && boundary > nextStart )
		{
			boundary = nextStart;
		}

		if ( boundary <= start )
		{
			return 0;
		}

		return boundary - start;
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
	this->TextAddress = 0;
	this->TextSize = 0;
	this->TextFileOffset = 0;
	this->TextFileSize = 0;
	this->DataAddress = 0;
	this->DataSize = 0;
	this->DataFileOffset = 0;
	this->DataFileSize = 0;
	this->RodataAddress = 0;
	this->RodataSize = 0;
	this->RodataFileOffset = 0;
	this->RodataFileSize = 0;
	this->BssAddress = 0;
	this->BssSize = 0;
	this->StackSize = 0;
	this->HeapSize = 0;
    this->sectionHeaders = 0;
}

/* 原来V6++的PEParser */
PEParser::PEParser(unsigned long peAddress)
{
	this->peAddress = peAddress + 0xC0000000;   // pe头的虚地址
	this->EntryPointAddress = 0;
	this->TextAddress = 0;
	this->TextSize = 0;
	this->TextFileOffset = 0;
	this->TextFileSize = 0;
	this->DataAddress = 0;
	this->DataSize = 0;
	this->DataFileOffset = 0;
	this->DataFileSize = 0;
	this->RodataAddress = 0;
	this->RodataSize = 0;
	this->RodataFileOffset = 0;
	this->RodataFileSize = 0;
	this->BssAddress = 0;
	this->BssSize = 0;
	this->StackSize = 0;
	this->HeapSize = 0;
	this->sectionHeaders = 0;
}

unsigned int PEParser::Relocate(Inode* p_inode, int sharedText)
{
	/* 用户镜像改为按需缺页加载：Relocate 仅保留兼容入口，不再提前触碰用户页。 */
	(void)p_inode;
	(void)sharedText;

	delete [] this->sectionHeaders;
	this->sectionHeaders = 0;
	return 0;
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

	unsigned long mappedTextSize = ComputeSectionMappedSize(
		this->ntHeader,
		this->sectionHeaders,
		this->ntHeader.FileHeader.NumberOfSections,
		this->TEXT_SECTION_IDX);
	if ( mappedTextSize != 0 )
	{
		this->TextSize = mappedTextSize;
	}

	this->TextFileOffset = this->sectionHeaders[this->TEXT_SECTION_IDX].PointerToRawData;
	this->TextFileSize = this->sectionHeaders[this->TEXT_SECTION_IDX].SizeOfRawData;
	if ( this->TextFileSize > this->TextSize )
	{
		this->TextFileSize = this->TextSize;
	}

	this->DataAddress =
		ntHeader.OptionalHeader.BaseOfData + ntHeader.OptionalHeader.ImageBase;
	this->DataFileOffset =
		this->sectionHeaders[this->DATA_SECTION_IDX].PointerToRawData;
	this->DataFileSize =
		this->sectionHeaders[this->DATA_SECTION_IDX].SizeOfRawData;
	unsigned long mappedDataSize = ComputeSectionMappedSize(
		this->ntHeader,
		this->sectionHeaders,
		this->ntHeader.FileHeader.NumberOfSections,
		this->DATA_SECTION_IDX);
	if ( mappedDataSize != 0 && this->DataFileSize > mappedDataSize )
	{
		this->DataFileSize = mappedDataSize;
	}

	unsigned long dataRawEnd =
		this->sectionHeaders[this->DATA_SECTION_IDX].VirtualAddress +
		this->sectionHeaders[this->DATA_SECTION_IDX].Misc.VirtualSize;
	if ( dataRawEnd <
			this->sectionHeaders[this->DATA_SECTION_IDX].VirtualAddress +
			this->sectionHeaders[this->DATA_SECTION_IDX].SizeOfRawData )
	{
		dataRawEnd =
			this->sectionHeaders[this->DATA_SECTION_IDX].VirtualAddress +
			this->sectionHeaders[this->DATA_SECTION_IDX].SizeOfRawData;
	}

	this->RodataAddress = 0;
	this->RodataSize = 0;
	this->RodataFileOffset = 0;
	this->RodataFileSize = 0;
	unsigned long rodataSectionStart = 0;
	unsigned long rodataRawOffset = 0;
	unsigned long rodataRawSize = 0;
	if ( ntHeader.FileHeader.NumberOfSections > this->RDATA_SECTION_IDX )
	{
		this->RodataAddress =
			this->sectionHeaders[this->RDATA_SECTION_IDX].VirtualAddress +
			ntHeader.OptionalHeader.ImageBase;
		rodataSectionStart = this->RodataAddress;
		rodataRawOffset = this->sectionHeaders[this->RDATA_SECTION_IDX].PointerToRawData;
		rodataRawSize = this->sectionHeaders[this->RDATA_SECTION_IDX].SizeOfRawData;
		this->RodataSize =
			this->sectionHeaders[this->RDATA_SECTION_IDX].Misc.VirtualSize;
		if ( this->RodataSize < this->sectionHeaders[this->RDATA_SECTION_IDX].SizeOfRawData )
		{
			this->RodataSize = this->sectionHeaders[this->RDATA_SECTION_IDX].SizeOfRawData;
		}
	}

	unsigned long mappedRodataSize = this->RodataSize;
	if ( ntHeader.FileHeader.NumberOfSections > this->RDATA_SECTION_IDX )
	{
		mappedRodataSize = ComputeSectionMappedSize(
			this->ntHeader,
			this->sectionHeaders,
			this->ntHeader.FileHeader.NumberOfSections,
			this->RDATA_SECTION_IDX);
	}

	this->BssAddress = 0;
	this->BssSize = 0;
	unsigned long dataEnd = dataRawEnd;
	if ( ntHeader.FileHeader.NumberOfSections > this->BSS_SECTION_IDX )
	{
		this->BssAddress =
			this->sectionHeaders[this->BSS_SECTION_IDX].VirtualAddress +
			ntHeader.OptionalHeader.ImageBase;
		this->BssSize =
			this->sectionHeaders[this->BSS_SECTION_IDX].Misc.VirtualSize;
		if ( this->BssSize < this->sectionHeaders[this->BSS_SECTION_IDX].SizeOfRawData )
		{
			this->BssSize = this->sectionHeaders[this->BSS_SECTION_IDX].SizeOfRawData;
		}

		unsigned long bssEnd =
			this->sectionHeaders[this->BSS_SECTION_IDX].VirtualAddress + this->BssSize;
		if ( bssEnd > dataEnd )
		{
			dataEnd = bssEnd;
		}
	}

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

	ComputeSharedRodataRange(
		this->DataAddress,
		this->DataSize,
		this->RodataAddress,
		mappedRodataSize,
		this->RodataAddress,
		this->RodataSize);

	if ( this->RodataSize != 0 &&
		rodataRawSize != 0 &&
		this->RodataAddress >= rodataSectionStart )
	{
		unsigned long offsetInRodataSection =
			this->RodataAddress - rodataSectionStart;
		if ( offsetInRodataSection < rodataRawSize )
		{
			this->RodataFileOffset = rodataRawOffset + offsetInRodataSection;
			unsigned long readable = rodataRawSize - offsetInRodataSection;
			this->RodataFileSize = this->RodataSize;
			if ( this->RodataFileSize > readable )
			{
				this->RodataFileSize = readable;
			}
		}
	}

    StackSize = ntHeader.OptionalHeader.SizeOfStackCommit;
    HeapSize = ntHeader.OptionalHeader.SizeOfHeapCommit;

    EntryPointAddress = ntHeader.OptionalHeader.AddressOfEntryPoint +
                    ntHeader.OptionalHeader.ImageBase;

	return true;
}
