#include "Video.h"

#if defined(OOS_KERNEL_DIAGNOSE_TO_DEBUGCON)
#include "IOPort.h"
#endif

namespace
{
	static const unsigned long DIAGNOSE_VIDEO_MEMORY_BASE = 0xC00B8000;
	static const unsigned int DIAGNOSE_DEFAULT_ROWS = 10;
	#if defined(OOS_KERNEL_DIAGNOSE_TO_DEBUGCON)
	static const unsigned short QEMU_DEBUGCON_PORT_E9 = 0xE9;
	static const unsigned short QEMU_DEBUGCON_PORT_402 = 0x402;
	#endif
	typedef __builtin_va_list DiagnoseVaList;

	/*
	 * 内核以 -nostdinc 构建，不能直接包含 <stdarg.h>，
	 * 因此这里使用编译器内建的可变参数支持。
	 */
#define DIAGNOSE_VA_START(ap, last) __builtin_va_start(ap, last)
#define DIAGNOSE_VA_ARG(ap, type) __builtin_va_arg(ap, type)
#define DIAGNOSE_VA_END(ap) __builtin_va_end(ap)

	static inline void MirrorDiagnoseToDebugCon(char ch)
	{
#if defined(OOS_KERNEL_DIAGNOSE_TO_DEBUGCON)
		IOPort::OutByte(QEMU_DEBUGCON_PORT_E9, (unsigned char)ch);
		IOPort::OutByte(QEMU_DEBUGCON_PORT_402, (unsigned char)ch);
#else
		(void)ch;
#endif
	}

}

unsigned short* Diagnose::m_VideoMemory = (unsigned short *)DIAGNOSE_VIDEO_MEMORY_BASE;
unsigned int Diagnose::m_Row = DIAGNOSE_DEFAULT_ROWS;
unsigned int Diagnose::m_Column = 0;

unsigned int Diagnose::ROWS = DIAGNOSE_DEFAULT_ROWS;

bool Diagnose::trace_on = true;

char Diagnose::m_History[Diagnose::HISTORY_LINE_COUNT][Diagnose::COLUMNS];
unsigned int Diagnose::m_TotalLines = 1;
unsigned int Diagnose::m_CurrentLine = 0;
unsigned int Diagnose::m_ViewTopLine = 0;
bool Diagnose::m_IsBrowsing = false;
bool Diagnose::m_HistoryReady = false;

void Diagnose::EnsureHistoryReady()
{
	if ( m_HistoryReady )
	{
		return;
	}

	for ( unsigned int i = 0; i < HISTORY_LINE_COUNT; ++i )
	{
		for ( unsigned int j = 0; j < COLUMNS; ++j )
		{
			m_History[i][j] = ' ';
		}
	}

	m_HistoryReady = true;
	m_TotalLines = 1;
	m_CurrentLine = 0;
	m_ViewTopLine = 0;
	m_IsBrowsing = false;
	m_Column = 0;

	if ( ROWS == 0 )
	{
		m_Row = SCREEN_ROWS;
	}
	else
	{
		m_Row = RegionTopRow();
	}
}

unsigned int Diagnose::RegionTopRow()
{
	if ( ROWS == 0 )
	{
		return SCREEN_ROWS;
	}

	if ( ROWS >= SCREEN_ROWS )
	{
		return 0;
	}

	return SCREEN_ROWS - ROWS;
}

unsigned int Diagnose::EarliestLine()
{
	if ( m_TotalLines > HISTORY_LINE_COUNT )
	{
		return m_TotalLines - HISTORY_LINE_COUNT;
	}

	return 0;
}

unsigned int Diagnose::TailTopLine()
{
	if ( ROWS == 0 )
	{
		return m_CurrentLine;
	}

	if ( m_CurrentLine + 1 > ROWS )
	{
		return m_CurrentLine + 1 - ROWS;
	}

	return 0;
}

unsigned int Diagnose::LogicalToHistoryLine(unsigned int logicalLine)
{
	return logicalLine % HISTORY_LINE_COUNT;
}

void Diagnose::ClearLogicalLine(unsigned int logicalLine)
{
	unsigned int historyLine = LogicalToHistoryLine(logicalLine);

	for ( unsigned int i = 0; i < COLUMNS; ++i )
	{
		m_History[historyLine][i] = ' ';
	}
}

void Diagnose::SyncCursorByView()
{
	if ( ROWS == 0 )
	{
		m_Row = SCREEN_ROWS;
		return;
	}

	unsigned int visibleRow = 0;

	if ( m_CurrentLine < m_ViewTopLine )
	{
		visibleRow = 0;
	}
	else if ( m_CurrentLine >= m_ViewTopLine + ROWS )
	{
		visibleRow = ROWS - 1;
	}
	else
	{
		visibleRow = m_CurrentLine - m_ViewTopLine;
	}

	m_Row = RegionTopRow() + visibleRow;
}

void Diagnose::RenderViewport()
{
	if ( ROWS == 0 )
	{
		m_Row = SCREEN_ROWS;
		return;
	}

	unsigned int earliest = EarliestLine();
	unsigned int tailTop = TailTopLine();

	if ( m_ViewTopLine < earliest )
	{
		m_ViewTopLine = earliest;
	}

	if ( m_ViewTopLine > tailTop )
	{
		m_ViewTopLine = tailTop;
	}

	unsigned int topRow = RegionTopRow();

	for ( unsigned int row = 0; row < ROWS; ++row )
	{
		unsigned int logicalLine = m_ViewTopLine + row;

		for ( unsigned int col = 0; col < COLUMNS; ++col )
		{
			char ch = ' ';

			if ( logicalLine < m_TotalLines && logicalLine >= earliest )
			{
				ch = m_History[LogicalToHistoryLine(logicalLine)][col];
			}

			m_VideoMemory[(topRow + row) * COLUMNS + col] = (unsigned char)ch | Diagnose::COLOR;
		}
	}

	SyncCursorByView();
}

Diagnose::Diagnose()
{
	//全部都是static成员变量，所以没有什么需要在构造函数中初始化的。
}

Diagnose::~Diagnose()
{
	//this is an empty dtor
}


void Diagnose::TraceOn()
{
	Diagnose::trace_on = 1;
}

void Diagnose::TraceOff()
{
	Diagnose::trace_on = 0;
}

/*
	能够输出格式化后的字符串，目前只能识别一些%d %x  %s 和%n;
	没有检查错误功能，% 和 值匹配要自己注意。
*/
void Diagnose::Write(const char* fmt, ...)
{
	if ( false == Diagnose::trace_on )
	{
		return;
	}

	if ( 0 == Diagnose::ROWS )
	{
		return;
	}

	EnsureHistoryReady();

	// Diagnose::RepairState();

	DiagnoseVaList args;
	DIAGNOSE_VA_START(args, fmt);
	const char * ch = fmt;
	
	while(1)
	{
		while(*ch != '%' && *ch != '\n')
		{
			if(*ch == '\0')
			{
				DIAGNOSE_VA_END(args);
				return;
			}
			if(*ch == '\n')
				break;
			/*注意： '\n'是一个单一字符，而不是'\\'和 ‘n'两个字符的相加， 
			譬如在字符串"\nHello World!!"中如果比较 if(*ch == '\\' && *(ch+1) == '\n' ) 的话，
			会死的狠惨的！*/
			WriteChar(*ch++);
		}
		
		ch++;	//skip the '%' or '\n'   

		if(*ch == 'd' || *ch == 'x')
		{//%d 或 %x 格式来输出，当然要添加八进制和二进制也很容易，但用处不大。
			if(*ch == 'x')
			{
				unsigned int value = DIAGNOSE_VA_ARG(args, unsigned int);
				Write("0x");   //as prefix for HEX value
				PrintInt(value, 16);
			}
			else
			{
				int value = DIAGNOSE_VA_ARG(args, int);
				if ( value < 0 )
				{
					WriteChar('-');
					PrintInt((unsigned int)(-(value + 1)) + 1, 10);
				}
				else
				{
					PrintInt((unsigned int)value, 10);
				}
			}
			ch++;	//skip the 'd' or 'x'
		}
		
		else if(*ch == 's')
		{//%s 格式来输出
			ch++;	//skip the 's'
			char *str = DIAGNOSE_VA_ARG(args, char *);
			while(char tmp = *str++)
			{
				WriteChar(tmp);
			}
		}
		else /* if(*(ch-1) == '\n') */
		{
			Diagnose::NextLine();
		}
	}
}

/*
	参考UNIX v6中的函数prf.c/printn(n,b)
	此函数的功能是将一个值value以base进制的方式显示出来。
*/
void Diagnose::PrintInt(unsigned int value, int base)
{
	int i;
	
	if((i = value / base) != 0)
		PrintInt(i ,base);

	unsigned int digit = value % base;
	WriteChar(digit < 10 ? (char)('0' + digit) : (char)('A' + digit - 10));
}

void Diagnose::NextLine()
{
	if ( 0 == Diagnose::ROWS )
	{
		return;
	}

	EnsureHistoryReady();
	MirrorDiagnoseToDebugCon('\n');
	m_CurrentLine += 1;
	if ( m_TotalLines < m_CurrentLine + 1 )
	{
		m_TotalLines = m_CurrentLine + 1;
	}
	ClearLogicalLine(m_CurrentLine);

	if ( !m_IsBrowsing )
	{
		m_ViewTopLine = TailTopLine();
	}

	m_Column = 0;
	RenderViewport();
}

void Diagnose::WriteChar(const char ch)
{
	if ( 0 == Diagnose::ROWS )
	{
		return;
	}

	EnsureHistoryReady();

	if(Diagnose::m_Column >= Diagnose::COLUMNS)
	{
		NextLine();
	}

	m_History[LogicalToHistoryLine(m_CurrentLine)][m_Column] = ch;
	MirrorDiagnoseToDebugCon(ch);

	if ( !m_IsBrowsing )
	{
		SyncCursorByView();
		m_VideoMemory[m_Row * COLUMNS + m_Column] = (unsigned char) ch | Diagnose::COLOR;
	}

	Diagnose::m_Column++;

	if ( Diagnose::m_Column >= Diagnose::COLUMNS )
	{
		NextLine();
	}
}

void Diagnose::ClearScreen()
{
	EnsureHistoryReady();

	for ( unsigned int i = 0; i < HISTORY_LINE_COUNT; ++i )
	{
		for ( unsigned int j = 0; j < COLUMNS; ++j )
		{
			m_History[i][j] = ' ';
		}
	}

	m_TotalLines = 1;
	m_CurrentLine = 0;
	m_ViewTopLine = 0;
	m_IsBrowsing = false;
	m_Column = 0;

	if ( 0 == Diagnose::ROWS )
	{
		Diagnose::m_Row = Diagnose::SCREEN_ROWS;
		return;
	}

	Diagnose::m_Row = RegionTopRow();
	RenderViewport();
}

void Diagnose::ScrollUpOneLine()
{
	if ( 0 == Diagnose::ROWS )
	{
		return;
	}

	EnsureHistoryReady();
	unsigned int earliest = EarliestLine();

	if ( m_ViewTopLine > earliest )
	{
		m_IsBrowsing = true;
		m_ViewTopLine -= 1;
		RenderViewport();
	}
}

void Diagnose::ScrollDownOneLine()
{
	if ( 0 == Diagnose::ROWS )
	{
		return;
	}

	EnsureHistoryReady();
	unsigned int tailTop = TailTopLine();

	if ( m_ViewTopLine < tailTop )
	{
		m_IsBrowsing = true;
		m_ViewTopLine += 1;
		RenderViewport();
		return;
	}

	FollowTail();
}

void Diagnose::FollowTail()
{
	if ( 0 == Diagnose::ROWS )
	{
		return;
	}

	EnsureHistoryReady();
	m_IsBrowsing = false;
	m_ViewTopLine = TailTopLine();
	RenderViewport();
}
