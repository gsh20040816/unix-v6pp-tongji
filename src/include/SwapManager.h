#ifndef SWAP_MANAGER_H
#define SWAP_MANAGER_H

#include "PageManager.h"

class SwapManager
{
public:
	static const unsigned int BLOCKS_PER_PAGE = PageManager::PAGE_SIZE / 512;
	static const unsigned int INVALID_SWAP_SLOT = 0xffffffff;
	static const unsigned int MAX_SWAP_SLOT_COUNT = 1024;

	struct SwapSlotInfo
	{
		unsigned char used;
		unsigned int mapCount;
		unsigned int residentCount;
		unsigned long residentFrame;
	};

public:
	SwapManager();
	void Initialize();

	bool IsInitialized() const;
	unsigned int AllocateSlot(unsigned int mapCount);
	void AddSlotReference(unsigned int slot);
	void FreeSlot(unsigned int slot);
	void ReleaseSlotReference(unsigned int slot);
	void ReleaseResidentReference(unsigned int slot);
	unsigned int GetSlotMapCount(unsigned int slot) const;
	void SetSlotReferenceCount(unsigned int slot, unsigned int mapCount);
	bool RegisterResident(unsigned int slot, unsigned long frameAddress);
	unsigned long GetResidentFrame(unsigned int slot) const;
	bool HasResidentFrame(unsigned int slot) const;
	bool ReadPage(unsigned int slot, unsigned long physicalAddress);
	bool WritePage(unsigned int slot, unsigned long physicalAddress);

private:
	bool IsValidSlot(unsigned int slot) const;
	int SlotToBlock(unsigned int slot) const;

private:
	unsigned int m_SlotCount;
	int m_SwapBeginBlock;
	SwapSlotInfo m_Slots[MAX_SWAP_SLOT_COUNT];
	bool m_Initialized;
};

#endif
