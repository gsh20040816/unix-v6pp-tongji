#include "New.h"
#include "Utility.h"

#ifndef _SIZE_T
typedef __SIZE_TYPE__ size_t;
#define _SIZE_T
#endif

namespace std
{
	class bad_alloc {};
}

KernelAllocator* g_pAllocator;

namespace
{
	static const unsigned long KERNEL_HEAP_MAGIC = 0x4B4E4C4D; /* KNLM */
	static const unsigned long KERNEL_HEAP_ALIGNMENT = 8;

	struct KernelHeapBlockHeader
	{
		unsigned long magic;
		unsigned long totalSize;
	};

	static unsigned long AlignUp(unsigned long value, unsigned long alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}

	static void* KernelHeapAllocate(unsigned long size)
	{
		if ( g_pAllocator == NULL )
		{
			Utility::Panic("Kernel new before allocator initialized");
		}

		if ( size == 0 )
		{
			size = 1;
		}

		if ( size > (~0ul) - (unsigned long)sizeof(KernelHeapBlockHeader) )
		{
			return 0;
		}

		unsigned long totalSize = AlignUp(size + (unsigned long)sizeof(KernelHeapBlockHeader),
			KERNEL_HEAP_ALIGNMENT);
		unsigned long blockAddress = g_pAllocator->AllocMemory(totalSize);
		if ( blockAddress == 0 )
		{
			return 0;
		}

		KernelHeapBlockHeader* header = (KernelHeapBlockHeader*)blockAddress;
		header->magic = KERNEL_HEAP_MAGIC;
		header->totalSize = totalSize;
		return (void*)(blockAddress + sizeof(KernelHeapBlockHeader));
	}

	static void KernelHeapRelease(void* p)
	{
		if ( p == 0 )
		{
			return;
		}

		if ( g_pAllocator == NULL )
		{
			Utility::Panic("Kernel delete before allocator initialized");
		}

		unsigned long userAddress = (unsigned long)p;
		KernelHeapBlockHeader* header =
			(KernelHeapBlockHeader*)(userAddress - sizeof(KernelHeapBlockHeader));
		if ( header->magic != KERNEL_HEAP_MAGIC ||
			header->totalSize < sizeof(KernelHeapBlockHeader) )
		{
			Utility::Panic("Invalid kernel delete pointer");
		}

		unsigned long totalSize = header->totalSize;
		header->magic = 0;
		header->totalSize = 0;
		g_pAllocator->FreeMemeory(totalSize, (unsigned long)header);
	}
}

void set_kernel_allocator(KernelAllocator* pAllocator)
{
	g_pAllocator = pAllocator;
}

void* operator new (size_t size) throw(std::bad_alloc)
{
	return KernelHeapAllocate((unsigned long)size);
}

void operator delete (void* p) throw()
{
	KernelHeapRelease(p);
}

void* operator new[] (size_t size) throw(std::bad_alloc)
{
	return KernelHeapAllocate((unsigned long)size);
}

void operator delete[] (void* p) throw()
{
	KernelHeapRelease(p);
}

