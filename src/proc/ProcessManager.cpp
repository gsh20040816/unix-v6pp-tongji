#include "ProcessManager.h"
#include "Machine.h"
#include "User.h"
#include "Kernel.h"
#include "Video.h"
#include "Utility.h"
#include "PEParser.h"
#include "Regs.h"
#include "MemoryDescriptor.h"

unsigned int ProcessManager::m_NextUniquePid = 0;

namespace
{
	static int g_NewProcTraceCount = 0;

	static void DumpProcBrief(const char* tag, Process* p)
	{
		if ( p == NULL )
		{
			Diagnose::Write("%s: <null>\n", tag);
			return;
		}

		Diagnose::Write(
			"%s: pid=%d ppid=%d stat=%d flag=%x addr=%x size=%x text=%x pri=%d time=%d\n",
			tag,
			p->p_pid,
			p->p_ppid,
			p->p_stat,
			p->p_flag,
			p->p_addr,
			p->p_size,
			p->p_textp,
			p->p_pri,
			p->p_time);
	}
}

ProcessManager::ProcessManager()
{
	CurPri = 0;
	RunRun = 0;
	ExeCnt = 0;
	SwtchNum = 0;
}

ProcessManager::~ProcessManager()
{
}

void ProcessManager::Initialize()
{
	//nothing to do here
}

void ProcessManager::SetupProcessZero()
{
	//初始化Process#0的Process和User结构
	Process* pProcZero = &(this->process[0]);
	pProcZero->p_stat = Process::SRUN;
	pProcZero->p_flag = Process::SLOAD | Process::SSYS;
	pProcZero->p_nice = 0;
	pProcZero->p_time = 0;
	pProcZero->p_pid = NextUniquePid();
	//除ppda区与核心栈外，进程没有用户态部分
	pProcZero->p_size = 0x1000;
	pProcZero->p_addr = PROCESS_ZERO_PPDA_ADDRESS;
	pProcZero->p_textp = NULL;
	pProcZero->p_xstat = 0;
	pProcZero->p_utime = 0;
	pProcZero->p_stime = 0;
	pProcZero->p_cutime = 0;
	pProcZero->p_cstime = 0;
	pProcZero->p_memory.UseKernelAddressSpace(
		(PageDirectory*)(Machine::PAGE_DIRECTORY_BASE_ADDRESS + Machine::KERNEL_SPACE_START_ADDRESS));

	User& u = Kernel::Instance().GetUser();
	u.u_procp = pProcZero;
}

unsigned int ProcessManager::NextUniquePid()
{
	return ProcessManager::m_NextUniquePid++;
}

int ProcessManager::NewProc()
{
	Process* child = 0;
	for (int i = 0; i < ProcessManager::NPROC; i++ )
	{
		if ( process[i].p_stat == Process::SNULL )
		{
			child = &process[i];
			break;
		}
	}
	if ( !child ) 
	{
		Utility::Panic("No Proc Entry!");
	}

	User& u = Kernel::Instance().GetUser();
	Process* current = (Process*)u.u_procp;

	if ( g_NewProcTraceCount < 24 )
	{
		Diagnose::Write("NewProc begin #%d u=%x u_procp=%x rsav=[%x,%x]\n",
			g_NewProcTraceCount, &u, u.u_procp, u.u_rsav[0], u.u_rsav[1]);
		DumpProcBrief("NewProc current", current);
	}

	current->Clone(*child);
	child->p_flag |= Process::SFORKRET;

	SaveU(u.u_rsav);

	PageTable* pgTable = current->p_memory.GetUserPageTableArray();
	child->p_memory.Initialize();
	child->p_memory.CloneFrom(current->p_memory);

	UserPageManager& userPageManager = Kernel::Instance().GetUserPageManager();
	child->p_addr = userPageManager.AllocMemory(ProcessManager::USIZE);
	if ( child->p_addr == 0 )
	{
		Utility::Panic("Out of user memory for child u area");
	}

	if ( g_NewProcTraceCount < 24 )
	{
		DumpProcBrief("NewProc child after Clone", child);
		Diagnose::Write("NewProc child alloc u-page=%x current_u_page=%x\n",
			child->p_addr, current->p_addr);
	}

	child->p_size = current->p_size;

	X86Assembly::CLI();
	if ( current->p_memory.GetUserPageTableArray() == NULL )
	{
		KernelPageManager& kernelPgMgr = Kernel::Instance().GetKernelPageManager();
		unsigned long scratchPage = kernelPgMgr.AllocMemory(ProcessManager::USIZE);
		if ( scratchPage == 0 )
		{
			Utility::Panic("Out of kernel memory for bootstrap copy");
		}

		Utility::MemCopy(
			current->p_addr + Machine::KERNEL_SPACE_START_ADDRESS,
			scratchPage + Machine::KERNEL_SPACE_START_ADDRESS,
			ProcessManager::USIZE);
		Utility::CopyToPhysical(
			child->p_addr,
			(void*)(scratchPage + Machine::KERNEL_SPACE_START_ADDRESS),
			ProcessManager::USIZE);
		kernelPgMgr.FreeMemory(ProcessManager::USIZE, scratchPage);
	}
	else
	{
		Utility::CopyPage(current->p_addr, child->p_addr);
	}

	unsigned long userProcOffset =
		(unsigned long)((char*)&u.u_procp - (char*)&u);
	if ( g_NewProcTraceCount < 24 )
	{
		Diagnose::Write(
			"NewProc patch child u_procp at phys=%x offset=%x\n",
			child->p_addr,
			userProcOffset);
	}
	Utility::CopyToPhysical(child->p_addr + userProcOffset, &child, sizeof(child));

	if ( pgTable == NULL )
	{
		if ( child->p_memory.MaterializeBootstrapStack() == false )
		{
			Utility::Panic("Bootstrap child address space failed");
		}
	}
	else
	{
		if ( child->p_memory.CloneResidentPagesFrom(current->p_memory) == false )
		{
			Utility::Panic("Clone child address space failed");
		}
	}

	child->p_memory.BuildPageTablesForImage();
	child->p_memory.Activate();

	if ( g_NewProcTraceCount < 24 )
	{
		DumpProcBrief("NewProc child ready", child);
		Diagnose::Write("NewProc child pgdir=%x userpt=%x\n",
			child->p_memory.GetPageDirectoryPointer(),
			child->p_memory.GetUserPageTableArray());
	}
	g_NewProcTraceCount++;

	X86Assembly::STI();

	return 0;
}

/* 在进程切换的过程中，根本没有用到TSS */
int ProcessManager::Swtch()
{	
	//Diagnose::Write("Start Swtch()\n");
	User& u = Kernel::Instance().GetUser();
	SaveU(u.u_rsav);

	/* 0#进程上台*/
	Process* procZero = &process[0];

	/* 
	 * 将SwtchUStruct()和RetU()作为临界区，防止被中断打断。
	 * 如果在RetU()恢复esp之后，尚未恢复ebp时，中断进入会导致
	 * esp和ebp分别指向两个不同进程的核心栈中位置。 good comment！
	 *
	 * 为什么，由0#进程承担挑选就绪进程上台的操作？
	 * 单从进程切换的角度，完全可以由下台进程挑选就绪进程上台。 但是，考虑时钟中断。
	 * 一秒末的 例行处理，最好系统idle时，其次是在执行应用程序过程中；不可以放在内核执行过程中。
	 * 如何判断？
	 * 内核idle的标志：  0#进程在睡眠态执行idle()子程序。
	 * 看 TimeInterrupt.cpp的Line 82.
	 * 如是，必须由0#进程执行select()。
	 *
	 */
	X86Assembly::CLI();
	SwtchUStruct(procZero);
	/* 原来的宏调用是这样写的   RetU(u0)，u0参数没用到，会引起歧义，删除 */
	RetU();
	X86Assembly::STI();

	/* 挑选最适合上台的进程 */
	Process* selected = Select();
	//Diagnose::Write("Process id = %d Selected!\n", selected->p_pid);

	/* 恢复被保存进程的现场 */
	if ( selected->p_flag & Process::SFORKRET )
	{
		selected->p_flag &= ~Process::SFORKRET;
		X86Assembly::CLI();
		SwtchUStruct(selected);
		RetU();
		return 1;
	}

	X86Assembly::CLI();
	SwtchUStruct(selected);

	RetU();
	X86Assembly::STI();

	User& newu = Kernel::Instance().GetUser();
	newu.WritePageTable();
	
	/* 
	 * 被fork出的进程在上台之前会在被调度上台时返回1，
	 * 并同时返回到NewProc()执行的地址
	 */
	return 1;
}

void ProcessManager::Sched()
{
	User& u = Kernel::Instance().GetUser();

	/*
	 * 非交换模式下，0#进程作为idle进程运行：
	 * 进入可中断睡眠，让正常调度路径挑选可运行进程上台。
	 */
	while ( true )
	{
		u.u_procp->Sleep((unsigned long)u.u_procp, ProcessManager::PSWP);
	}
}

void ProcessManager::Wait()
{
	int i;
	User& u = Kernel::Instance().GetUser();
	
	Diagnose::Write("Process %d finding dead son. They are ",u.u_procp->p_pid);
	while(true)
	{
		bool hasChild = false;
		for ( i = 0; i < NPROC; i++ )
		{
			Process* child = &process[i];
			if ( u.u_procp->p_pid == child->p_ppid )
			{
				Diagnose::Write("Process %d (Status:%d)  ",child->p_pid,child->p_stat);
				hasChild = true;
				/* 睡眠等待直至子进程结束 */
				if( Process::SZOMB == child->p_stat )
				{
					/* wait()系统调用返回子进程的pid */
					u.u_ar0[User::EAX] = child->p_pid;

					/* 把子进程的时间加到父进程上 */
					u.u_cstime += child->p_cstime + child->p_stime;
					u.u_cutime += child->p_cutime + child->p_utime;

					int* pInt = (int *)u.u_arg[0];
					/* 获取子进程exit(int status)的返回值 */
					*pInt = child->p_xstat;

					child->p_stat = Process::SNULL;
					child->p_pid = 0;
					child->p_ppid = -1;
					child->p_sig = 0;
					child->p_flag = 0;
					child->p_addr = 0;
					child->p_size = 0;
					child->p_textp = NULL;
					child->p_xstat = 0;
					child->p_utime = 0;
					child->p_stime = 0;
					child->p_cutime = 0;
					child->p_cstime = 0;

					Diagnose::Write("end wait\n");
					return;
				}
			}
		}
		if (true == hasChild)
		{
			/* 睡眠等待直至子进程结束 */
			Diagnose::Write("wait until child process Exit! ");
			u.u_procp->Sleep((unsigned long)u.u_procp, ProcessManager::PWAIT);
			Diagnose::Write("end sleep\n");
			continue;	/* 回到外层while(true)循环 */
		}
		else
		{
			/* 不存在需要等待结束的子进程，设置出错码，wait()返回 */
			u.u_error = User::ECHILD;
			break;	/* Get out of while loop */
		}
	}
}

void ProcessManager::Fork()
{
	User& u = Kernel::Instance().GetUser();
	Process* child = NULL;;

	/* 寻找空闲的process项，作为子进程的进程控制块 */
	for ( int i = 0; i < ProcessManager::NPROC; i++ )
	{
		if ( this->process[i].p_stat == Process::SNULL )
		{
			child = &this->process[i];
			break;
		}
	}
	if ( child == NULL )
	{
		/* 没有空闲process表项，返回 */
		u.u_error = User::EAGAIN;
		return;
	}

	if ( this->NewProc() )	/* 子进程返回1，父进程返回0 */
	{
		/* 子进程fork()系统调用返回0 */
		u.u_procp->p_memory.DisplayPageTable();
		u.u_ar0[User::EAX] = 0;
		u.u_cstime = 0;
		u.u_stime = 0;
		u.u_cutime = 0;
		u.u_utime = 0;
	}
	else
	{
		/* 父进程进程fork()系统调用返回子进程PID */
		u.u_ar0[User::EAX] = child->p_pid;
	}

	return;
}

extern "C" void runtime();
extern "C" void ExecShell();

/* 终于敢称为 V6 的 exec实现。缺憾：不支持 ISUID 比特 */
void ProcessManager::Exec()
{
	Inode* pInode;
	Text* pText;
	User& u = Kernel::Instance().GetUser();
	FileManager& fileMgr = Kernel::Instance().GetFileManager();
	UserPageManager& userPgMgr = Kernel::Instance().GetUserPageManager();
	KernelPageManager& kernelPgMgr = Kernel::Instance().GetKernelPageManager();

	Diagnose::Write("Process %d execing\n",u.u_procp->p_pid);
	pInode = fileMgr.NameI(FileManager::NextChar, FileManager::OPEN);
	if ( NULL == pInode )	//搜索目录失败
	{
		return;
	}

	/* 如果同时进行图像改换的进程数超出限制，则先进入睡眠 */
	while( this->ExeCnt >= NEXEC )
	{
		u.u_procp->Sleep((unsigned long)&ExeCnt, ProcessManager::EXPRI);
	}
	this->ExeCnt++;

	/* 进程必需拥有可执行文件的执行权限，且被执行的只能是一般文件。 */
	if ( fileMgr.Access(pInode, Inode::IEXEC) || (pInode->i_mode & Inode::IFMT) != 0 )
	{
		fileMgr.m_InodeTable->IPut(pInode);
		if ( this->ExeCnt >= NEXEC )
		{
			WakeUpAll((unsigned long)&ExeCnt);
		}
		this->ExeCnt--;
		return;
	}

	PEParser parser;

    if ( parser.HeaderLoad(pInode)==false )
    {
        fileMgr.m_InodeTable->IPut(pInode);
        return;
    }

	/* 
	 * 分配内存用于存放用户程序运行需要的参数argc，argv[]，这些参数由exec()系统调用传入，
	 * 位于进程图像改换前的用户栈中，将参数备份到fakeStack中，然后可以释放原进程图像，
	 * 分配好新进程图像之后，再将fakeStack中的备份参数拷贝到新进程的用户栈中。
	 * 注意：这里必须在ConfigureExecutableLayout()之前完成参数备份，否则旧用户栈映射会被清空。
	 */
	//unsigned long fakeStack = kernelPgMgr.AllocMemory(parser.StackSize);
	int allocLength = (parser.StackSize + PageManager::PAGE_SIZE * 2 - 1) >> 13 << 13;
	unsigned long fakeStack = kernelPgMgr.AllocMemory(allocLength);

	int argc = u.u_arg[1];
	char** argv = (char **)u.u_arg[2];

	/* esp定位到栈底 */
	unsigned int esp = MemoryDescriptor::USER_SPACE_SIZE;
	/* 使用核心态页表映射，所以在物理地址上加0xC0000000构成线性地址 */
	unsigned long desAddress = fakeStack + allocLength + 0xC0000000;
	//unsigned long desAddress = fakeStack + parser.StackSize + 0xC0000000;
	int length;

	/* 复制argv[]指针数组指向的命令行参数字符串 */
	for (int i = 0; i < argc; i++ )
	{
		length = 0;
		/* 计算参数字符串长度，length不含'\0' */
		while( NULL != argv[i][length] )
		{
			length++;
		}
		desAddress = desAddress - (length + 1);
		/* 拷贝时将'\0'一起拷贝过去 */
		Utility::MemCopy((unsigned long)argv[i], desAddress, length + 1);
		/* 将参数字符串在新进程图像用户栈中的起始位置存入argv[i]，用户栈位于进程逻辑地址空间0x800000的底部 */
		esp = esp - (length + 1);
		argv[i] = (char *)esp;
	}

	/* 后续存放的是int型数值，这里以16字节边界对齐 */
	desAddress = desAddress & 0xFFFFFFF0;
	esp = esp & 0xFFFFFFF0;

	/* 复制argc和argv[] */
	int endValue = 0;
	desAddress -= sizeof(endValue);
	esp -= sizeof(endValue);
	/* 向用户栈中写入endValue作为argv[]的结束 */
	Utility::MemCopy((unsigned long)&endValue, desAddress, sizeof(endValue));

	desAddress -= argc * sizeof(int);
	esp -= argc * sizeof(int);
	/* 写入argv[]的内容 */
	Utility::MemCopy((unsigned long)argv, desAddress, argc * sizeof(int));

	/* 令endValue指向当前栈中argv[]的起始地址，即argv[]入栈完毕后当前栈顶地址 */
	endValue = esp;
	desAddress -= sizeof(int);
	esp -= sizeof(int);
	Utility::MemCopy((unsigned long)&endValue, desAddress, sizeof(int));

	/* 最后入栈argc */
	desAddress -= sizeof(int);
	esp -= sizeof(int);
	Utility::MemCopy((unsigned long)&argc, desAddress, sizeof(int));	/* Done! */

	/* 获取分析PE头结构得到正文段的起始地址、长度 */
	if ( u.u_procp->p_memory.ConfigureExecutableLayout(parser.EntryPointAddress,
			parser.TextAddress,
			parser.TextSize,
			parser.DataAddress,
			parser.DataSize,
			parser.StackSize) == false )
	{
		kernelPgMgr.FreeMemory(allocLength, fakeStack);
		fileMgr.m_InodeTable->IPut(pInode);
		u.u_error = User::ENOMEM;
		return;
	}


	/* 释放原进程图像的共享正文段，数据段，堆栈段 */
	u.u_procp->p_memory.ReleaseResidentPages(false);
	if ( u.u_procp->p_textp != NULL )
	{
		u.u_procp->p_textp->XFree();
		u.u_procp->p_textp = NULL;
	}
	u.u_procp->p_size = ProcessManager::USIZE;

	pText = NULL;
	/* 分配一个空闲Text结构，或者和其它进程共享同一正文段 */
	for ( int i = 0; i < ProcessManager::NTEXT; i++ )
	{
		if ( NULL == this->text[i].x_iptr )     /* 记下找到的第一个空闲text结构 */
		{
			if ( NULL == pText )
			{
				pText = &(this->text[i]);
			}
		}
		else if ( pInode == this->text[i].x_iptr )		/* 如果，这不是一个空闲text结构，看一下text结构指向的可执行文件是exec系统调用要执行的应用程序吗？ */
		{
			this->text[i].x_count++;
			this->text[i].x_ccount++;
			u.u_procp->p_textp = &(this->text[i]);
			pText = NULL;	/* 与其它进程共享同一正文段，则pText重新清零，否则指向一空闲Text结构 */
			break;
		}
	}


	int sharedText = 0;

	/* 没有可共享的现成Text结构，进行相应初始化 */
	if ( NULL != pText )
	{
		/* 
		 * 此处i_count++用于平衡XFree()函数中的IPut(x_iptr)；倘若只有Exec()开始处
		 * 调用NameI()函数中IGet()，以及Exec()结尾处IPut()释放exe文件的Inode回到空闲Inode表，
		 * 极端情况下：若后续进程很快也Exec()，获取空闲Inode恰好是之前加载的exe文件释放的Inode，
		 * 则会错误地判断：pInode (当前exe对应Inode) == this->text[i].x_iptr(之前exe文件Inode)，
		 * 导致和之前进程共享同一Text结构，即同一正文段，而实际上本该是两个独立的程序。
		 */
		pInode->i_count++;

		pText->x_ccount = 1;
		pText->x_count = 1;
		pText->x_iptr = pInode;
		pText->x_size = u.u_procp->p_memory.GetCodeSize();
		/* 为正文段分配内存，而具体正文段内容的读入需要等到建立页表映射之后，再从mapAddress地址起始的exe文件中读入 */
		pText->x_caddr = userPgMgr.AllocMemory(pText->x_size);
		pText->x_daddr = 0;
		/* 建立u区和Text结构的勾连关系 */
		u.u_procp->p_textp = pText;
	}
	else
	{
		pText = u.u_procp->p_textp;
		sharedText = 1;
	}

	unsigned int newSize = ProcessManager::USIZE + u.u_procp->p_memory.GetWritableSize();
	if ( false == u.u_procp->p_memory.CheckUserSpace() )
	{
		return;   // out of virtual space. fail
	}

	u.u_procp->p_size = newSize;
	Diagnose::Write("Process %x, p_addr %x, x_addr %x, p_size %x, x_size %x\n",
			u.u_procp->p_pid,u.u_procp->p_addr,u.u_procp->p_textp->x_caddr,u.u_procp->p_size,u.u_procp->p_textp->x_size);

	if ( u.u_procp->p_memory.MaterializeExecutableImage(u.u_procp->p_textp->x_caddr) == false )
	{
		u.u_error = User::ENOMEM;
		return;
	}
	u.u_procp->p_memory.BuildPageTablesForImage();
	u.u_procp->p_memory.Activate();
    u.u_procp->p_memory.DisplayPageTable();

	/* 从exe文件中依次读入.text段、.data段、.rdata段、.bss段 */
	parser.Relocate(pInode, sharedText);

	/* 将fakeStack中备份的用户栈参数复制到新进程图像的用户栈中 */
	//Utility::MemCopy(fakeStack | 0xC0000000, MemoryDescriptor::USER_SPACE_SIZE - parser.StackSize, parser.StackSize);
	Utility::MemCopy(fakeStack + allocLength - parser.StackSize | 0xC0000000,
		MemoryDescriptor::USER_SPACE_SIZE - parser.StackSize, parser.StackSize);
	/* 释放用于读入exe文件和备份用户栈参数的内存：mapAddress和fakeStack */
	kernelPgMgr.FreeMemory(allocLength, fakeStack);

	/* 
	  * 将runtime()、SignalHandler()函数拷贝到进程用户态地址空间0x00000000线性地址处，runtime()
	  * 用于ring0退出到ring3特权级之后执行的代码，SignalHandler()为进程的信号处理函数入口，负责
	  * 调用具体信号的Handler。每一个进程0x00000000线性地址处都应该有一份独立的runtime()及SignalHandler()
	  * 函数副本！
	  */
//	unsigned char* runtimeSrc = (unsigned char*)runtime;
//	unsigned char* runtimeDst = 0x00000000;
//	for (unsigned int i = 0; i < (unsigned long)ExecShell - (unsigned long)runtime; i++)
//	{
//		*runtimeDst++ = *runtimeSrc++;
//	}

	/* 释放Inode，减少ExeCnt计数值 */
	fileMgr.m_InodeTable->IPut(pInode);
	if ( this->ExeCnt >= NEXEC )
	{
		WakeUpAll((unsigned long)&ExeCnt);
	}
	this->ExeCnt--;

	/* 用默认的方式处理信号  */
	for (int i = 0; i < u.NSIG ; i++)
	{
		u.u_signal[i] = 0;
	}

	/* u.u_ar0 指向 pt_regs::eax，不能再把“字节偏移”直接当成 unsigned int 下标使用。 */
	struct pt_regs* pRegs =
		(struct pt_regs*)((char*)u.u_ar0 - (unsigned long)&((struct pt_regs*)0)->eax);
	pRegs->edi = 0;
	pRegs->esi = 0;
	pRegs->edx = 0;
	pRegs->ecx = 0;
	pRegs->ebx = 0;

	/* 将exe程序的入口地址放入核心栈现场保护区中的EAX作为系统调用返回值，这个是runtime要用  */
	u.u_ar0[User::EAX] = u.u_procp->p_memory.GetEntryPoint();

	/* exec 改写的是同一份系统调用返回现场，数据段寄存器也要切回用户态。 */
	pRegs->pad1 = Machine::USER_DATA_SEGMENT_SELECTOR;	/* gs */
	pRegs->pad2 = Machine::USER_DATA_SEGMENT_SELECTOR;	/* fs */
	pRegs->xds = Machine::USER_DATA_SEGMENT_SELECTOR;
	pRegs->xes = Machine::USER_DATA_SEGMENT_SELECTOR;
	
	/* 构造出Exec()系统调用的退出环境，使之退出到ring3时，开始执行user code */
	struct pt_context* pContext = (struct pt_context *)u.u_arg[4];
	pContext->eip = 0x00000000;	/* 退出到ring3特权级下从线性地址0x00000000处runtime()开始执行 */
	//pContext->eip = parser.EntryPointAddress;
	pContext->xcs = Machine::USER_CODE_SEGMENT_SELECTOR;
	pContext->eflags = 0x200;	/* 此项是否篡改无关紧要 */
	pContext->esp = esp;
	pContext->xss = Machine::USER_DATA_SEGMENT_SELECTOR;
}

Process* ProcessManager::Select ()
{
	/* 前一次选中上台进程 */
	static int lastSelect = 0;
	
	while (true)
	{
		int priority = 256;
		int best = -1;	/* 本轮搜索找到的最合适上台进程 */

		this->RunRun = 0;

		/* 搜索优先级最高的可运行进程 */
		for ( int count = 0; count < NPROC ; count++ )
		{
			/* 从上一次被选中进程的下一个开始回环扫描，而不是每次从0#进程开始，保证各进程机会均等 */
			int i = (lastSelect + 1 + count) % NPROC;
			if ( Process::SRUN == process[i].p_stat && (process[i].p_flag & Process::SLOAD) != 0 )
			{
				if ( process[i].p_pri < priority )
				{
					best = i;
					priority = process[i].p_pri;
				}
			}
		}
		if ( -1 == best )
		{
			__asm__ __volatile__("hlt");
			continue;
		}

		SwtchNum++;
		if ( SwtchNum & 0x80000000 ) 
		{
			SwtchNum = 0;	/* 计数溢出变为负数后，重置为零 */
		}
		/* 如果选出优先级最高的可运行进程 */
		this->CurPri = priority;
		lastSelect = best;
		//Diagnose::Write("Process %d is running!",best);
		return &process[best];

	}
}

void ProcessManager::Kill()
{
	User& u = Kernel::Instance().GetUser();
	int pid = u.u_arg[0];
	int signal = u.u_arg[1];
	bool flag = false;

	for ( int i = 0; i < ProcessManager::NPROC; i++ )
	{
		/* 不允许发送信号给进程自身 */
		if ( u.u_procp == &process[i] )
		{
			continue;
		}
		/* 不是信号的接收方目标进程，继续搜寻 */
		if ( pid != 0 && process[i].p_pid != pid)
		{
			continue;
		}
		/* pid为0，则将信号发送至与发送进程同一终端的所有进程，0#进程不包括在内 */
		if ( pid == 0 && (process[i].p_ttyp != u.u_procp->p_ttyp || i == 0 ) )
		{
			continue;
		}
		/* 除非是超级用户，否则要求发送、接收进程u.uid相同，即不可给其它用户进程发送信号 */
		if ( u.u_uid != 0 && u.u_uid != process[i].p_uid )
		{
			continue;
		}
		flag = true;
		/* 信号发送给满足条件的目标进程 */
		process[i].PSignal(signal);
	}
	if ( false == flag )
	{
		u.u_error = User::ESRCH;
	}
}

void ProcessManager::WakeUpAll(unsigned long chan)
{
	/* 唤醒系统中所有因chan而进入睡眠的进程 */
	for(int i = 0; i < ProcessManager::NPROC; i++)
	{
		if( this->process[i].IsSleepOn(chan) )
		{
			this->process[i].SetRun();
		}
	}
}

void ProcessManager::Signal( TTy* pTTy, int signal )
{
	for ( int i = 0; i < ProcessManager::NPROC; i++ )
	{
		if ( this->process[i].p_ttyp == pTTy )
		{
			this->process[i].PSignal(signal);
		}
	}
}
