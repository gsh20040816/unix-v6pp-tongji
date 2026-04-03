#include "TestBufferManager.h"
#include "../KernelInclude.h"

void PrintBuffer(Buf* pBuf)
{
	for(int i = 0; i < BufferManager::BUFFER_SIZE; i++)
	{
		Diagnose::Write("%x ", pBuf->b_addr[i]);
		if(i % 100 == 0) Delay();
	}
}

void ModifyBuffer(Buf* pBuf, int offset)
{
	for(int i = 0; i < BufferManager::BUFFER_SIZE; i++)
	{
		pBuf->b_addr[i] = 'a' + offset + i % 26;
	}
}

int CheckSumBuffer(Buf* pBuf)
{
	int sum = 0;
	for(int i = 0; i < BufferManager::BUFFER_SIZE; i++)
	{
		sum += pBuf->b_addr[i];
	}
	Diagnose::Write("CheckSum Buf addr of %x = %x\n", pBuf->b_addr, sum);
	return sum;
}

bool BreadTest()
{
	Diagnose::Write("Start Test Bread...\n");
	Buf* pBuf;

	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();
	pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, 0);
	CheckSumBuffer(pBuf);

	/* 一定要Brelse() */
	bufMgr.Brelse(pBuf);

	pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, 0);
	Diagnose::Write("Read same block second time!\n");
	CheckSumBuffer(pBuf);

	return true;
}

bool RepeatReadTest()
{
	Diagnose::Write("Repeated Read Test Start...\n");
	Buf* pBuf;
	unsigned long addr;
	/* 15个缓存循环利用，读取多个字符块，但别超过c.img的扇区数20,160 Sectors */
	int repeat = 3000;

	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();

	pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, 0);
	/* 一定要Brelse() */
	bufMgr.Brelse(pBuf);
	/* 记录1st buffer的地址 */
	addr = (unsigned long)pBuf->b_addr;

	int nbuffer = 0;
	for( int blkno = 0; blkno < repeat; blkno++)
	{
		pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, blkno);
		//CheckSumBuffer(pBuf);
		nbuffer = ( (unsigned long)pBuf->b_addr - addr )/BufferManager::BUFFER_SIZE;
		Diagnose::Write("Using Buffer[%d]...\n", nbuffer);
		//Delay();
		bufMgr.Brelse(pBuf);
	}

	if( nbuffer == (repeat - 1) % BufferManager::NBUF)
	{
		return true;
	}
	else
	{
		Diagnose::Write("Test Failed!\n");
		while(1);
	}
}


bool WriteTest()
{
	/* read 0# sector, modify it and write back */
	Diagnose::Write("Repeated Read Test Start...\n");
	Buf* pBuf;

	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();
	
	pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, 0);

	ModifyBuffer(pBuf, 0);
	int checksum = CheckSumBuffer(pBuf);
	Diagnose::Write("%s\n", pBuf->b_addr);

	bufMgr.Bwrite(pBuf);
	Diagnose::Write("Write Done!\n");	/* Go and check the c.img */

	return true;
}

bool TestBufferManager()
{
	/* read 1# sector,  modify it and write to 2#, read 2# sector for check */
	Diagnose::Write("Start Test Buffer Manager...\n");
	Buf* pBuf;
	
	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();
	pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, 1);

	pBuf->b_blkno = 2;
	ModifyBuffer(pBuf, 1);
	int checksum = CheckSumBuffer(pBuf);

	bufMgr.Bawrite(pBuf);
	Diagnose::Write("Bwrite Done!\n");
	
	/* 
	Violate the buf, force the following Bread() to conduct a real I/O,  
	rather than fetch data from the exist buf!!
	*/
	pBuf->b_dev = -1;

	pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, 2);
	if( CheckSumBuffer(pBuf) == checksum )
	{
		Delay();
		return true;
	}
	else
	{
		Diagnose::Write("Test Failed...\n");
		while (1);
	}
}

