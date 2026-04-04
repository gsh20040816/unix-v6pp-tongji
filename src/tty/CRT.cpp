#include "CRT.h"
#include "IOPort.h"

unsigned short* CRT::m_VideoMemory = (unsigned short *)(0xB8000 + 0xC0000000);
unsigned int CRT::m_CursorX = 0;
unsigned int CRT::m_CursorY = 0;
char* CRT::m_Position = 0;
char* CRT::m_BeginChar = 0;

unsigned int CRT::ROWS = 15;

char CRT::m_History[CRT::HISTORY_LINE_COUNT][CRT::COLUMNS];
unsigned int CRT::m_TotalLines = 1;
unsigned int CRT::m_CurrentLine = 0;
unsigned int CRT::m_ViewTopLine = 0;
bool CRT::m_IsBrowsing = false;
bool CRT::m_HistoryReady = false;

void CRT::EnsureHistoryReady()
{
	if ( CRT::m_HistoryReady )
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
	m_CursorX = 0;
	m_CursorY = 0;
}

unsigned int CRT::EarliestLine()
{
	if ( m_TotalLines > HISTORY_LINE_COUNT )
	{
		return m_TotalLines - HISTORY_LINE_COUNT;
	}
	return 0;
}

unsigned int CRT::TailTopLine()
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

unsigned int CRT::LogicalToHistoryLine(unsigned int logicalLine)
{
	return logicalLine % HISTORY_LINE_COUNT;
}

void CRT::ClearLogicalLine(unsigned int logicalLine)
{
	unsigned int historyLine = LogicalToHistoryLine(logicalLine);
	for ( unsigned int i = 0; i < COLUMNS; ++i )
	{
		m_History[historyLine][i] = ' ';
	}
}

void CRT::SyncCursorByView()
{
	if ( ROWS == 0 )
	{
		m_CursorY = 0;
		return;
	}

	if ( m_CurrentLine < m_ViewTopLine )
	{
		m_CursorY = 0;
	}
	else if ( m_CurrentLine >= m_ViewTopLine + ROWS )
	{
		m_CursorY = ROWS - 1;
	}
	else
	{
		m_CursorY = m_CurrentLine - m_ViewTopLine;
	}
}

void CRT::RenderViewport()
{
	if ( ROWS == 0 )
	{
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
			m_VideoMemory[row * COLUMNS + col] = (unsigned char)ch | CRT::COLOR;
		}
	}

	SyncCursorByView();
	MoveCursor(m_CursorX, m_CursorY);
}

void CRT::CRTStart(TTy* pTTy)
{
	EnsureHistoryReady();
	if ( m_IsBrowsing )
	{
		FollowTail();
	}

	char ch;
	if ( 0 == CRT::m_BeginChar)
	{
		m_BeginChar = pTTy->t_outq.CurrentChar();
	}
	if ( 0 == m_Position )
	{
		m_Position = m_BeginChar;
	}

	while ( (ch = pTTy->t_outq.GetChar()) != TTy::GET_ERROR )
	{
		switch (ch)
		{
		case '\n':
			NextLine();
			CRT::m_BeginChar = pTTy->t_outq.CurrentChar();
			m_Position = CRT::m_BeginChar;
			break;

		case 0x15:
			//del_line();
			break;

		case '\b':
			if ( m_Position != CRT::m_BeginChar )
			{
				BackSpace();
				m_Position--;
			}
			break;

		case '\t':
			Tab();
			m_Position++;
			break;

		default:	/* 在屏幕上回显普通字符 */
			WriteChar(ch);
			m_Position++;
			break;
		}
   }
}

void CRT::MoveCursor(unsigned int col, unsigned int row)
{
	if ( ROWS == 0 )
	{
		return;
	}

	if ( col >= CRT::COLUMNS || row >= CRT::ROWS )
	{
		return;
	}

	/* 计算光标偏移量 */
	unsigned short cursorPosition = row * CRT::COLUMNS + col;

	/* 选择 r14和r15寄存器，分别为光标位置的高8位和低8位 */
	IOPort::OutByte(CRT::VIDEO_ADDR_PORT, 14);
	IOPort::OutByte(CRT::VIDEO_DATA_PORT, cursorPosition >> 8);
	IOPort::OutByte(CRT::VIDEO_ADDR_PORT, 15);
	IOPort::OutByte(CRT::VIDEO_DATA_PORT, cursorPosition & 0xFF);
}

void CRT::NextLine()
{
	EnsureHistoryReady();

	m_CursorX = 0;
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

	RenderViewport();
}

void CRT::BackSpace()
{
	EnsureHistoryReady();

	if ( m_CursorX == 0 )
	{
		if ( m_CurrentLine == 0 )
		{
			return;
		}
		m_CurrentLine -= 1;
		m_CursorX = CRT::COLUMNS;
	}

	m_CursorX -= 1;
	m_History[LogicalToHistoryLine(m_CurrentLine)][m_CursorX] = ' ';

	if ( !m_IsBrowsing )
	{
		m_ViewTopLine = TailTopLine();
	}
	RenderViewport();

}

void CRT::Tab()
{
	EnsureHistoryReady();

	m_CursorX &= 0xFFFFFFF8;	/* 向左对齐到前一个Tab边界 */
	m_CursorX += 8;
	// const int TabWidth = 10;
	// m_CursorX -= m_CursorX % TabWidth;
	// m_CursorX += TabWidth;
	if ( m_CursorX >= CRT::COLUMNS )
		NextLine();
	else
	{
		SyncCursorByView();
		MoveCursor(m_CursorX, m_CursorY);
	}
}

void CRT::WriteChar(char ch)
{
	EnsureHistoryReady();

	if ( m_CursorX >= CRT::COLUMNS )
	{
		NextLine();
	}

	m_History[LogicalToHistoryLine(m_CurrentLine)][m_CursorX] = ch;
	if ( !m_IsBrowsing )
	{
		SyncCursorByView();
		m_VideoMemory[m_CursorY * CRT::COLUMNS + m_CursorX] = (unsigned char) ch | CRT::COLOR;
	}

	m_CursorX++;
	
	if ( m_CursorX >= CRT::COLUMNS )
	{
		NextLine();
	}
	else
	{
		SyncCursorByView();
		MoveCursor(m_CursorX, m_CursorY);
	}
}

void CRT::ClearScreen()
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
	m_CursorX = 0;
	m_CursorY = 0;

	RenderViewport();
}

void CRT::ScrollUpOneLine()
{
	EnsureHistoryReady();
	unsigned int earliest = EarliestLine();

	if ( m_ViewTopLine > earliest )
	{
		m_IsBrowsing = true;
		m_ViewTopLine -= 1;
		RenderViewport();
	}
}

void CRT::ScrollDownOneLine()
{
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

void CRT::FollowTail()
{
	EnsureHistoryReady();
	m_IsBrowsing = false;
	m_ViewTopLine = TailTopLine();
	RenderViewport();
}

bool CRT::IsBrowsingHistory()
{
	return m_IsBrowsing;
}

