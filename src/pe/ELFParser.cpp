#include "ELFParser.h"
#include "PageManager.h"
#include "User.h"
#include "Kernel.h"

namespace
{
    static const unsigned char ELF_MAGIC_0 = 0x7f;
    static const unsigned char ELF_MAGIC_1 = 'E';
    static const unsigned char ELF_MAGIC_2 = 'L';
    static const unsigned char ELF_MAGIC_3 = 'F';
    static const unsigned char ELF_CLASS_32 = 1;
    static const unsigned char ELF_LITTLE_ENDIAN = 1;
    static const unsigned short ELF_TYPE_EXEC = 2;
    static const unsigned short ELF_MACHINE_I386 = 3;
    static const unsigned long ELF_PT_GNU_STACK = 0x6474e551UL;

    static const unsigned long DEFAULT_STACK_SIZE = 0x4000;
    static const unsigned long DEFAULT_HEAP_SIZE = 0x100000;

    struct ELF32ProgramHeader
    {
        unsigned long p_type;
        unsigned long p_offset;
        unsigned long p_vaddr;
        unsigned long p_paddr;
        unsigned long p_filesz;
        unsigned long p_memsz;
        unsigned long p_flags;
        unsigned long p_align;
    };

    static unsigned long AlignDownToPage(unsigned long value)
    {
        return value & ~(PageManager::PAGE_SIZE - 1);
    }

    static unsigned long AlignUpToPage(unsigned long value)
    {
        return (value + PageManager::PAGE_SIZE - 1) & ~(PageManager::PAGE_SIZE - 1);
    }

    static bool ReadInodeRange(Inode* inode,
        unsigned long offset,
        void* buffer,
        unsigned long size)
    {
        if ( inode == NULL || buffer == NULL )
        {
            return false;
        }

        User& u = Kernel::Instance().GetUser();
        u.u_IOParam.m_Base = (unsigned char*)buffer;
        u.u_IOParam.m_Offset = offset;
        u.u_IOParam.m_Count = size;
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

    static bool StringEquals(const char* lhs, const char* rhs)
    {
        if ( lhs == NULL || rhs == NULL )
        {
            return false;
        }

        while ( *lhs != '\0' && *rhs != '\0' )
        {
            if ( *lhs != *rhs )
            {
                return false;
            }
            ++lhs;
            ++rhs;
        }

        return *lhs == *rhs;
    }

    static const char* ResolveSectionName(const ELF32SectionHeader* headers,
        unsigned short sectionCount,
        unsigned short index,
        const char* sectionNameTable,
        unsigned long sectionNameTableSize)
    {
        if ( headers == NULL || sectionNameTable == NULL || index >= sectionCount )
        {
            return NULL;
        }

        unsigned long nameOffset = headers[index].sh_name;
        if ( nameOffset >= sectionNameTableSize )
        {
            return NULL;
        }

        return sectionNameTable + nameOffset;
    }

    static unsigned long ComputeSectionMappedSize(const ELF32SectionHeader* sectionHeaders,
        unsigned short sectionCount,
        unsigned short sectionIndex)
    {
        if ( sectionHeaders == NULL || sectionIndex >= sectionCount )
        {
            return 0;
        }

        const ELF32SectionHeader& section = sectionHeaders[sectionIndex];
        unsigned long usedSize = section.sh_size;
        if ( usedSize == 0 )
        {
            return 0;
        }

        unsigned long start = section.sh_addr;
        unsigned long boundary = AlignUpToPage(start + usedSize);
        unsigned long currentSectionAddress = section.sh_addr;
        unsigned long nextStart = 0xffffffffUL;

        for ( unsigned short i = 0; i < sectionCount; ++i )
        {
            if ( i == sectionIndex )
            {
                continue;
            }

            unsigned long candidateAddress = sectionHeaders[i].sh_addr;
            if ( candidateAddress <= currentSectionAddress )
            {
                continue;
            }

            if ( candidateAddress < nextStart )
            {
                nextStart = candidateAddress;
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
}

ELFParser::ELFParser()
{
    this->m_ElfAddress = 0;
    this->m_SectionHeaders = 0;
    this->m_SectionNameTable = 0;
    this->ResetParsedResult();
}

ELFParser::ELFParser(unsigned long elfAddress)
{
    this->m_ElfAddress = elfAddress + 0xC0000000;
    this->m_SectionHeaders = 0;
    this->m_SectionNameTable = 0;
    this->ResetParsedResult();
}

void ELFParser::ResetParsedResult()
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
}

unsigned long ELFParser::Parse()
{
    return 0;
}

unsigned int ELFParser::Relocate(Inode* p_inode, int sharedText)
{
    (void)p_inode;
    (void)sharedText;

    delete [] this->m_SectionHeaders;
    this->m_SectionHeaders = 0;

    delete [] this->m_SectionNameTable;
    this->m_SectionNameTable = 0;

    return 0;
}

unsigned int ELFParser::Relocate()
{
    return 0;
}

bool ELFParser::HeaderLoad(Inode* p_inode)
{
    if ( this->m_SectionHeaders != 0 )
    {
        delete [] this->m_SectionHeaders;
        this->m_SectionHeaders = 0;
    }
    if ( this->m_SectionNameTable != 0 )
    {
        delete [] this->m_SectionNameTable;
        this->m_SectionNameTable = 0;
    }
    this->ResetParsedResult();

    if ( ReadInodeRange(p_inode, 0, &this->m_Header, sizeof(this->m_Header)) == false )
    {
        return false;
    }

    if ( this->m_Header.e_ident[0] != ELF_MAGIC_0 ||
        this->m_Header.e_ident[1] != ELF_MAGIC_1 ||
        this->m_Header.e_ident[2] != ELF_MAGIC_2 ||
        this->m_Header.e_ident[3] != ELF_MAGIC_3 )
    {
        return false;
    }

    if ( this->m_Header.e_ident[4] != ELF_CLASS_32 ||
        this->m_Header.e_ident[5] != ELF_LITTLE_ENDIAN )
    {
        return false;
    }

    if ( this->m_Header.e_type != ELF_TYPE_EXEC ||
        this->m_Header.e_machine != ELF_MACHINE_I386 )
    {
        return false;
    }

    unsigned long parsedStackSize = DEFAULT_STACK_SIZE;
    if ( this->m_Header.e_phoff != 0 &&
        this->m_Header.e_phnum != 0 &&
        this->m_Header.e_phentsize >= sizeof(ELF32ProgramHeader) )
    {
        for ( unsigned short i = 0; i < this->m_Header.e_phnum; ++i )
        {
            ELF32ProgramHeader ph;
            if ( ReadInodeRange(p_inode,
                    this->m_Header.e_phoff +
                        (unsigned long)i * this->m_Header.e_phentsize,
                    &ph,
                    sizeof(ph)) == false )
            {
                return false;
            }

            if ( ph.p_type == ELF_PT_GNU_STACK && ph.p_memsz != 0 )
            {
                parsedStackSize = AlignUpToPage(ph.p_memsz);
                break;
            }
        }
    }

    if ( parsedStackSize < PageManager::PAGE_SIZE )
    {
        parsedStackSize = PageManager::PAGE_SIZE;
    }

    if ( this->m_Header.e_shoff == 0 ||
        this->m_Header.e_shnum == 0 ||
        this->m_Header.e_shentsize != sizeof(ELF32SectionHeader) )
    {
        return false;
    }

    this->m_SectionHeaders = new ELF32SectionHeader[this->m_Header.e_shnum];
    if ( this->m_SectionHeaders == 0 )
    {
        return false;
    }

    unsigned long sectionTableSize =
        (unsigned long)this->m_Header.e_shnum * sizeof(ELF32SectionHeader);
    if ( ReadInodeRange(p_inode,
            this->m_Header.e_shoff,
            this->m_SectionHeaders,
            sectionTableSize) == false )
    {
        return false;
    }

    if ( this->m_Header.e_shstrndx >= this->m_Header.e_shnum )
    {
        return false;
    }

    const ELF32SectionHeader& sectionNameHeader =
        this->m_SectionHeaders[this->m_Header.e_shstrndx];
    if ( sectionNameHeader.sh_size == 0 )
    {
        return false;
    }

    this->m_SectionNameTable = new char[sectionNameHeader.sh_size + 1];
    if ( this->m_SectionNameTable == 0 )
    {
        return false;
    }

    if ( ReadInodeRange(p_inode,
            sectionNameHeader.sh_offset,
            this->m_SectionNameTable,
            sectionNameHeader.sh_size) == false )
    {
        return false;
    }
    this->m_SectionNameTable[sectionNameHeader.sh_size] = '\0';

    int textIndex = -1;
    int dataIndex = -1;
    int rodataIndex = -1;
    int bssIndex = -1;

    for ( unsigned short i = 0; i < this->m_Header.e_shnum; ++i )
    {
        const char* sectionName = ResolveSectionName(this->m_SectionHeaders,
            this->m_Header.e_shnum,
            i,
            this->m_SectionNameTable,
            sectionNameHeader.sh_size);
        if ( sectionName == NULL )
        {
            continue;
        }

        if ( textIndex < 0 && StringEquals(sectionName, ".text") )
        {
            textIndex = (int)i;
            continue;
        }

        if ( dataIndex < 0 && StringEquals(sectionName, ".data") )
        {
            dataIndex = (int)i;
            continue;
        }

        if ( rodataIndex < 0 && (StringEquals(sectionName, ".rodata") ||
            StringEquals(sectionName, ".rdata")) )
        {
            rodataIndex = (int)i;
            continue;
        }

        if ( bssIndex < 0 && StringEquals(sectionName, ".bss") )
        {
            bssIndex = (int)i;
            continue;
        }
    }

    if ( textIndex < 0 || dataIndex < 0 )
    {
        return false;
    }

    const ELF32SectionHeader& textHeader = this->m_SectionHeaders[textIndex];
    this->TextAddress = textHeader.sh_addr;
    this->TextSize = textHeader.sh_size;
    unsigned long mappedTextSize = ComputeSectionMappedSize(this->m_SectionHeaders,
        this->m_Header.e_shnum,
        (unsigned short)textIndex);
    if ( mappedTextSize != 0 )
    {
        this->TextSize = mappedTextSize;
    }
    this->TextFileOffset = textHeader.sh_offset;
    this->TextFileSize = (textHeader.sh_type == ELFParser::SHT_NOBITS) ? 0 : textHeader.sh_size;
    if ( this->TextFileSize > this->TextSize )
    {
        this->TextFileSize = this->TextSize;
    }

    const ELF32SectionHeader& dataHeader = this->m_SectionHeaders[dataIndex];
    this->DataAddress = dataHeader.sh_addr;
    this->DataFileOffset = dataHeader.sh_offset;
    this->DataFileSize = (dataHeader.sh_type == ELFParser::SHT_NOBITS) ? 0 : dataHeader.sh_size;

    unsigned long dataEnd = dataHeader.sh_addr + dataHeader.sh_size;
    unsigned long mappedDataSize = ComputeSectionMappedSize(this->m_SectionHeaders,
        this->m_Header.e_shnum,
        (unsigned short)dataIndex);
    if ( mappedDataSize != 0 )
    {
        dataEnd = dataHeader.sh_addr + mappedDataSize;
        if ( this->DataFileSize > mappedDataSize )
        {
            this->DataFileSize = mappedDataSize;
        }
    }

    unsigned long rodataSectionStart = 0;
    unsigned long rodataRawOffset = 0;
    unsigned long rodataRawSize = 0;
    unsigned long mappedRodataSize = 0;

    if ( rodataIndex >= 0 )
    {
        const ELF32SectionHeader& rodataHeader = this->m_SectionHeaders[rodataIndex];
        this->RodataAddress = rodataHeader.sh_addr;
        this->RodataSize = rodataHeader.sh_size;
        rodataSectionStart = rodataHeader.sh_addr;
        rodataRawOffset = rodataHeader.sh_offset;
        rodataRawSize = (rodataHeader.sh_type == ELFParser::SHT_NOBITS) ? 0 : rodataHeader.sh_size;

        mappedRodataSize = ComputeSectionMappedSize(this->m_SectionHeaders,
            this->m_Header.e_shnum,
            (unsigned short)rodataIndex);
        if ( mappedRodataSize == 0 )
        {
            mappedRodataSize = this->RodataSize;
        }

        unsigned long rodataEnd = this->RodataAddress + mappedRodataSize;
        if ( rodataEnd > dataEnd )
        {
            dataEnd = rodataEnd;
        }
    }

    if ( bssIndex >= 0 )
    {
        const ELF32SectionHeader& bssHeader = this->m_SectionHeaders[bssIndex];
        this->BssAddress = bssHeader.sh_addr;
        this->BssSize = bssHeader.sh_size;

        unsigned long bssEnd = this->BssAddress + this->BssSize;
        if ( bssEnd > dataEnd )
        {
            dataEnd = bssEnd;
        }
    }

    if ( dataEnd <= this->DataAddress )
    {
        return false;
    }
    this->DataSize = dataEnd - this->DataAddress;

    ComputeSharedRodataRange(this->DataAddress,
        this->DataSize,
        this->RodataAddress,
        mappedRodataSize,
        this->RodataAddress,
        this->RodataSize);

    if ( this->RodataSize != 0 &&
        rodataRawSize != 0 &&
        this->RodataAddress >= rodataSectionStart )
    {
        unsigned long offsetInRodataSection = this->RodataAddress - rodataSectionStart;
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

    this->StackSize = parsedStackSize;
    this->HeapSize = DEFAULT_HEAP_SIZE;
    this->EntryPointAddress = this->m_Header.e_entry;

    return true;
}
