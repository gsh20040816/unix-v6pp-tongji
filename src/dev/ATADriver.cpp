#include "ATADriver.h"
#include "BufferManager.h"
#include "Utility.h"
#include "IOPort.h"
#include "Kernel.h"
#include "DMA.h"
#include "Chip8259A.h"

namespace
{
/*
 * Bus-Master IDE DMA 对内存区域位置较敏感。
 * 使用 64KB 对齐的 bounce buffer + PRD 表，避免内核布局变化后
 * 某些地址组合导致 DMA 传输成功但数据落点异常。
 */
static unsigned char g_DmaBounceBuffer[BufferManager::BUFFER_SIZE] __attribute__((aligned(65536)));
static PRDTable g_DmaPRDTable __attribute__((aligned(65536)));

static unsigned long ToPhysicalAddress(const void* linearAddress)
{
	return ((unsigned long)linearAddress) & ~0xC0000000UL;
}
}

void ATADriver::ATAHandler(struct pt_regs *reg, struct pt_context *context)
{
	Buf* bp;
	Devtab* atab;
	short major = Utility::GetMajor(DeviceManager::ROOTDEV);

	BlockDevice& bdev = 
		Kernel::Instance().GetDeviceManager().GetBlockDevice(major);
	atab = bdev.d_tab;
	
	if( atab->d_active == 0 )
	{
		/* 可能收到无请求时的残留中断，仍需EOI避免中断线被卡住。 */
		IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);
		IOPort::OutByte(Chip8259A::SLAVE_IO_PORT_1, Chip8259A::EOI);
		return;		/* 没有请求项 */
	}

	bp = atab->d_actf;		/* 获取本次中断对应的I/O请求Buf */
	atab->d_active = 0;		/* 表示设备已经空闲 */

	/* 检查I/O操作执行过程中磁盘控制器或者DMA控制器是否出错 */
	if( ATADriver::IsError() || DMA::IsError() )
	{
		if(++atab->d_errcnt <= 10)
		{
			bdev.Start();
			IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);
			IOPort::OutByte(Chip8259A::SLAVE_IO_PORT_1, Chip8259A::EOI);
			return;
		}
		bp->b_flags |= Buf::B_ERROR;
	}
	else if ((bp->b_flags & Buf::B_READ) == Buf::B_READ)
	{
		Utility::IOMove(g_DmaBounceBuffer, bp->b_addr, bp->b_wcount);
	}
	
	atab->d_errcnt = 0;		/* 错误计数器归零 */
	atab->d_actf = bp->av_forw;		/* 从I/O请求队列中取出已完成的I/O请求Buf */
	Kernel::Instance().GetBufferManager().IODone(bp);	/* I/O结束善后工作 */
	bdev.Start();	/* 启动I/O请求队列中下一个I/O请求 */
	/* 对主、从8259A中断控制芯片分别发送EOI命令。 */
	IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);
	IOPort::OutByte(Chip8259A::SLAVE_IO_PORT_1, Chip8259A::EOI);
	return;
}

void ATADriver::DevStart(Buf* bp)
{
	if(bp == NULL)
	{
		Utility::Panic("Invalid Buf in DevStart()!");
	}

	if (ATADriver::IsControllerReady() == 0)
	{
		Utility::Panic("Disk Hang Up!");
	}

	if (bp->b_wcount != BufferManager::BUFFER_SIZE)
	{
		bp->b_flags |= Buf::B_ERROR;
		return;
	}

	short minor = Utility::GetMinor(bp->b_dev);
	int sectorCount = bp->b_wcount / BufferManager::BUFFER_SIZE;
	bool isRead = ((bp->b_flags & Buf::B_READ) == Buf::B_READ);

	PhysicalRegionDescriptor prd;
	prd.SetBaseAddress(ToPhysicalAddress(g_DmaBounceBuffer));
	prd.SetByteCount((unsigned short)bp->b_wcount);
	g_DmaPRDTable.SetPhysicalRegionDescriptor(0, prd, true);

	if (!isRead)
	{
		Utility::IOMove(bp->b_addr, g_DmaBounceBuffer, bp->b_wcount);
	}

	DMA::Reset();

	IOPort::OutByte(ATADriver::NSECTOR_PORT, sectorCount);
	IOPort::OutByte(ATADriver::BLKNO_PORT_1, bp->b_blkno & 0xFF);
	IOPort::OutByte(ATADriver::BLKNO_PORT_2, (bp->b_blkno >> 8) & 0xFF);
	IOPort::OutByte(ATADriver::BLKNO_PORT_3, (bp->b_blkno >> 16) & 0xFF);
	IOPort::OutByte(
		ATADriver::MODE_PORT,
		ATADriver::MODE_IDE | ATADriver::MODE_LBA28 | (minor << 4) | ((bp->b_blkno >> 24) & 0x0F));

	if (isRead)
	{
		DMA::Start(DMA::READ, g_DmaPRDTable.GetPRDTableBaseAddress());
		IOPort::OutByte(ATADriver::CMD_PORT, ATADriver::HD_DMA_READ);
	}
	else
	{
		DMA::Start(DMA::WRITE, g_DmaPRDTable.GetPRDTableBaseAddress());
		IOPort::OutByte(ATADriver::CMD_PORT, ATADriver::HD_DMA_WRITE);
	}
	return;
}

int ATADriver::IsControllerReady()
{
	int ticks = 10000;
	
	while(--ticks)
	{
		unsigned char status = IOPort::InByte(ATADriver::STATUS_PORT);
		if( (status & (ATADriver::HD_DEVICE_BUSY | ATADriver::HD_DEVICE_READY)) == ATADriver::HD_DEVICE_READY )
		{
			return ticks;
		}
	}
	return 0;	/* 控制器长时间无响应 */
}

bool ATADriver::IsError()
{
	unsigned char status = IOPort::InByte(ATADriver::STATUS_PORT);
	if( (status & ATADriver::HD_ERROR) == ATADriver::HD_ERROR )
	{
		return true;	/* 出错 */
	}
	return false;	/* 没有出错 */
}
