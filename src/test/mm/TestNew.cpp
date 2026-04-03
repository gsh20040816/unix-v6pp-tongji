#include "../TestUtility.h"
#include "TestNew.h"

#include "KernelAllocator.h"
#include "Allocator.h"
#include "New.h"

class DummyClass
{
public:
	DummyClass();
	~DummyClass();
	int buffer[100];
};

DummyClass::DummyClass()
{
	int i = 0;
	i++;
}

DummyClass::~DummyClass()
{
	int j = 0;
	j--;
}


bool TestNew()
{
	const unsigned int headerSize = 8;
	const unsigned int alignment = 8;
	Allocator allocator;
	KernelAllocator kAllocator(&allocator);
	kAllocator.Initialize();
	set_kernel_allocator(&kAllocator);

	unsigned char buffer[1024];
	kAllocator.map[0].m_AddressIdx = (unsigned long)&buffer;
	kAllocator.map[0].m_Size = 1024;

	
	DummyClass* p = 0;
	//Case1 new
	p = new DummyClass();
	unsigned int allocSize = sizeof(DummyClass) + headerSize;
	allocSize = (allocSize + alignment - 1) & ~(alignment - 1);
	PrintResult(
		"Case1", 
		(unsigned long)p == ((unsigned long)&buffer + headerSize)
		&& kAllocator.map[0].m_AddressIdx == (unsigned long)&buffer + allocSize
		&& kAllocator.map[0].m_Size == 1024 - allocSize
		);

	//Case2 delete
	delete p;
	PrintResult(
		"Case2",
		kAllocator.map[0].m_AddressIdx == (unsigned long)&buffer 
		&& kAllocator.map[0].m_Size == 1024
		);
	return true;
}
