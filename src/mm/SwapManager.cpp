#include "SwapManager.h"
#include "Kernel.h"
#include "FileSystem.h"
#include "DeviceManager.h"
#include "BufferManager.h"
#include "Utility.h"
#include "Machine.h"
#include "Assembly.h"

namespace
{
	static void CopyPhysicalToBuffer(unsigned long physicalAddress,
		unsigned int offset,
		unsigned char* buffer,
		unsigned int count)
	{
		PageTable* pageTable = NULL;
		User& u = Kernel::Instance().GetUser();
		if ( u.u_procp != NULL )
		{
			pageTable = u.u_procp->p_memory.GetUserPageTableByIndex(0);
		}
		if ( pageTable == NULL )
		{
			pageTable = Machine::Instance().GetUserPageTableArray();
		}

		PageTableEntry* userPageTable = (PageTableEntry*)pageTable;
		PageTableEntry original = userPageTable[1];
		userPageTable[1] = original;
		userPageTable[1].m_Present = 1;
		userPageTable[1].m_ReadWriter = 1;
		userPageTable[1].m_UserSupervisor = 1;
		userPageTable[1].m_PageBaseAddress = physicalAddress / PageManager::PAGE_SIZE;
		X86Assembly::FlushCurrentPageDirectory();

		Utility::MemCopy(PageManager::PAGE_SIZE + offset, (unsigned long)buffer, count);

		userPageTable[1] = original;
		X86Assembly::FlushCurrentPageDirectory();
	}
}

SwapManager::SwapManager()
{
	this->m_SlotCount = 0;
	this->m_SwapBeginBlock = 0;
	this->m_Initialized = false;
	for ( unsigned int i = 0; i < MAX_SWAP_SLOT_COUNT; ++i )
	{
		this->m_Slots[i].used = 0;
		this->m_Slots[i].mapCount = 0;
		this->m_Slots[i].residentCount = 0;
		this->m_Slots[i].residentFrame = 0;
	}
}

void SwapManager::Initialize()
{
	this->m_SwapBeginBlock = FileSystem::DATA_ZONE_END_SECTOR + 1;
	unsigned int swapBlocks =
		ATABlockDevice::NSECTOR > this->m_SwapBeginBlock ?
		(unsigned int)(ATABlockDevice::NSECTOR - this->m_SwapBeginBlock) : 0;
	this->m_SlotCount = swapBlocks / BLOCKS_PER_PAGE;
	if ( this->m_SlotCount > MAX_SWAP_SLOT_COUNT )
	{
		this->m_SlotCount = MAX_SWAP_SLOT_COUNT;
	}

	for ( unsigned int i = 0; i < MAX_SWAP_SLOT_COUNT; ++i )
	{
		this->m_Slots[i].used = 0;
		this->m_Slots[i].mapCount = 0;
		this->m_Slots[i].residentCount = 0;
		this->m_Slots[i].residentFrame = 0;
	}
	this->m_Initialized = this->m_SlotCount != 0;
}

bool SwapManager::IsInitialized() const
{
	return this->m_Initialized;
}

unsigned int SwapManager::AllocateSlot(unsigned int mapCount)
{
	if ( this->m_Initialized == false || mapCount == 0 )
	{
		return INVALID_SWAP_SLOT;
	}

	for ( unsigned int i = 0; i < this->m_SlotCount; ++i )
	{
		if ( this->m_Slots[i].used == 0 )
		{
			this->m_Slots[i].used = 1;
			this->m_Slots[i].mapCount = mapCount;
			this->m_Slots[i].residentCount = 0;
			this->m_Slots[i].residentFrame = 0;
			return i;
		}
	}

	return INVALID_SWAP_SLOT;
}

void SwapManager::AddSlotReference(unsigned int slot)
{
	if ( this->IsValidSlot(slot) == false || this->m_Slots[slot].used == 0 )
	{
		return;
	}
	++this->m_Slots[slot].mapCount;
}

void SwapManager::FreeSlot(unsigned int slot)
{
	if ( this->IsValidSlot(slot) == false )
	{
		return;
	}

	this->m_Slots[slot].used = 0;
	this->m_Slots[slot].mapCount = 0;
	this->m_Slots[slot].residentCount = 0;
	this->m_Slots[slot].residentFrame = 0;
}

void SwapManager::ReleaseSlotReference(unsigned int slot)
{
	if ( this->IsValidSlot(slot) == false || this->m_Slots[slot].used == 0 )
	{
		return;
	}

	if ( this->m_Slots[slot].mapCount != 0 )
	{
		--this->m_Slots[slot].mapCount;
	}
	if ( this->m_Slots[slot].residentCount > this->m_Slots[slot].mapCount )
	{
		this->m_Slots[slot].residentCount = this->m_Slots[slot].mapCount;
	}
	if ( this->m_Slots[slot].residentCount == 0 )
	{
		this->m_Slots[slot].residentFrame = 0;
	}
	if ( this->m_Slots[slot].mapCount == 0 )
	{
		this->FreeSlot(slot);
	}
}

void SwapManager::ReleaseResidentReference(unsigned int slot)
{
	if ( this->IsValidSlot(slot) == false || this->m_Slots[slot].used == 0 )
	{
		return;
	}

	if ( this->m_Slots[slot].residentCount != 0 )
	{
		--this->m_Slots[slot].residentCount;
	}
	if ( this->m_Slots[slot].mapCount != 0 )
	{
		--this->m_Slots[slot].mapCount;
	}
	if ( this->m_Slots[slot].residentCount == 0 )
	{
		this->m_Slots[slot].residentFrame = 0;
	}
	if ( this->m_Slots[slot].mapCount == 0 )
	{
		this->FreeSlot(slot);
	}
}

unsigned int SwapManager::GetSlotMapCount(unsigned int slot) const
{
	if ( this->IsValidSlot(slot) == false || this->m_Slots[slot].used == 0 )
	{
		return 0;
	}
	return this->m_Slots[slot].mapCount;
}

void SwapManager::SetSlotReferenceCount(unsigned int slot, unsigned int mapCount)
{
	if ( this->IsValidSlot(slot) == false || this->m_Slots[slot].used == 0 )
	{
		return;
	}

	if ( mapCount == 0 )
	{
		this->FreeSlot(slot);
		return;
	}

	this->m_Slots[slot].mapCount = mapCount;
	if ( this->m_Slots[slot].residentCount > mapCount )
	{
		this->m_Slots[slot].residentCount = mapCount;
	}
	if ( this->m_Slots[slot].residentCount == 0 )
	{
		this->m_Slots[slot].residentFrame = 0;
	}
}

bool SwapManager::RegisterResident(unsigned int slot, unsigned long frameAddress)
{
	if ( this->IsValidSlot(slot) == false || this->m_Slots[slot].used == 0 )
	{
		return false;
	}

	if ( this->m_Slots[slot].residentFrame == 0 )
	{
		this->m_Slots[slot].residentFrame = frameAddress;
	}
	else if ( this->m_Slots[slot].residentFrame != frameAddress )
	{
		return false;
	}

	if ( this->m_Slots[slot].residentCount < this->m_Slots[slot].mapCount )
	{
		++this->m_Slots[slot].residentCount;
	}
	return true;
}

unsigned long SwapManager::GetResidentFrame(unsigned int slot) const
{
	if ( this->IsValidSlot(slot) == false || this->m_Slots[slot].used == 0 )
	{
		return 0;
	}
	return this->m_Slots[slot].residentFrame;
}

bool SwapManager::HasResidentFrame(unsigned int slot) const
{
	return this->GetResidentFrame(slot) != 0;
}

bool SwapManager::ReadPage(unsigned int slot, unsigned long physicalAddress)
{
	if ( this->IsValidSlot(slot) == false || this->m_Slots[slot].used == 0 ||
		physicalAddress == 0 )
	{
		return false;
	}

	BufferManager& bufferManager = Kernel::Instance().GetBufferManager();
	for ( unsigned int i = 0; i < BLOCKS_PER_PAGE; ++i )
	{
		Buf* bp = bufferManager.Bread(DeviceManager::ROOTDEV,
			this->SlotToBlock(slot) + (int)i);
		if ( bp == NULL || (bp->b_flags & Buf::B_ERROR) != 0 )
		{
			if ( bp != NULL )
			{
				bufferManager.Brelse(bp);
			}
			return false;
		}
		Utility::CopyToPhysical(
			physicalAddress + i * BufferManager::BUFFER_SIZE,
			bp->b_addr,
			BufferManager::BUFFER_SIZE);
		bufferManager.Brelse(bp);
	}
	return true;
}

bool SwapManager::WritePage(unsigned int slot, unsigned long physicalAddress)
{
	if ( this->IsValidSlot(slot) == false || this->m_Slots[slot].used == 0 ||
		physicalAddress == 0 )
	{
		return false;
	}

	BufferManager& bufferManager = Kernel::Instance().GetBufferManager();
	for ( unsigned int i = 0; i < BLOCKS_PER_PAGE; ++i )
	{
		Buf* bp = bufferManager.GetBlk(DeviceManager::ROOTDEV,
			this->SlotToBlock(slot) + (int)i);
		if ( bp == NULL )
		{
			return false;
		}
		CopyPhysicalToBuffer(
			physicalAddress,
			i * BufferManager::BUFFER_SIZE,
			bp->b_addr,
			BufferManager::BUFFER_SIZE);
		bufferManager.Bwrite(bp);
	}
	return true;
}

bool SwapManager::IsValidSlot(unsigned int slot) const
{
	return this->m_Initialized && slot < this->m_SlotCount;
}

int SwapManager::SlotToBlock(unsigned int slot) const
{
	return this->m_SwapBeginBlock + (int)(slot * BLOCKS_PER_PAGE);
}
