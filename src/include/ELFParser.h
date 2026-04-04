#ifndef ELF_PARSER_H
#define ELF_PARSER_H

#include "INode.h"

struct ELF32Header
{
    unsigned char e_ident[16];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned long e_version;
    unsigned long e_entry;
    unsigned long e_phoff;
    unsigned long e_shoff;
    unsigned long e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
};

struct ELF32SectionHeader
{
    unsigned long sh_name;
    unsigned long sh_type;
    unsigned long sh_flags;
    unsigned long sh_addr;
    unsigned long sh_offset;
    unsigned long sh_size;
    unsigned long sh_link;
    unsigned long sh_info;
    unsigned long sh_addralign;
    unsigned long sh_entsize;
};

class ELFParser
{
public:
    static const unsigned long SHT_NOBITS = 8;

public:
    ELFParser();
    ELFParser(unsigned long elfAddress);
    unsigned long Parse();
    unsigned int Relocate();
    unsigned int Relocate(Inode* p_inode, int sharedText);

    bool HeaderLoad(Inode* p_inode);

public:
    unsigned long EntryPointAddress;

    unsigned long TextAddress;
    unsigned long TextSize;
    unsigned long TextFileOffset;
    unsigned long TextFileSize;

    unsigned long DataAddress;
    unsigned long DataSize;
    unsigned long DataFileOffset;
    unsigned long DataFileSize;

    unsigned long RodataAddress;
    unsigned long RodataSize;
    unsigned long RodataFileOffset;
    unsigned long RodataFileSize;

    unsigned long BssAddress;
    unsigned long BssSize;

    unsigned long StackSize;
    unsigned long HeapSize;

private:
    void ResetParsedResult();

private:
    unsigned long m_ElfAddress;
    ELF32Header m_Header;
    ELF32SectionHeader* m_SectionHeaders;
    char* m_SectionNameTable;
};

#endif
