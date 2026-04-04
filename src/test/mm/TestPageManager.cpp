#include "PageManager.h"
#include "../TestUtility.h"

bool TestPageManager()
{
	//Setup
	KernelPageManager manager;
	manager.Initialize();

	//TestCases
	unsigned long result = 0;
	unsigned long freeBefore = manager.GetFreePageCount();
	
	//Case1
	result = manager.AllocatePage();
	PrintResult(
		"Case1", 
		result == KernelPageManager::KERNEL_PAGE_POOL_START_ADDR &&
		manager.GetFreePageCount() + 1 == freeBefore
		);

	//Case2
	result = manager.FreePage(result);
	PrintResult(
		"Case2", 
		result == 0 && manager.GetFreePageCount() == freeBefore
		);

	//TearDown
	return true;
}
