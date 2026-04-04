//Video.h
#ifndef DIAGNOSE_H
#define DIAGNOSE_H

class Diagnose
{
public:
	static unsigned int ROWS;

	/* static const member */
	static const unsigned int COLUMNS = 80;
	static const unsigned short COLOR = 0x0B00;	/* char in bright CYAN */
	static const unsigned int SCREEN_ROWS = 25;	/* full screen rows */
	static const unsigned int HISTORY_LINE_COUNT = 512;

public:
	Diagnose();
	~Diagnose();

	static void TraceOn();
	static void TraceOff();

	static void Write(const char* fmt, ...);
	static void ClearScreen();
	static void ScrollUpOneLine();
	static void ScrollDownOneLine();
	static void FollowTail();

	private:	
		static void PrintInt(unsigned int value, int base);
		static void NextLine();
		static void WriteChar(const char ch);
		static void RepairState();
		static void EnsureHistoryReady();
		static unsigned int RegionTopRow();
		static unsigned int EarliestLine();
		static unsigned int TailTopLine();
		static unsigned int LogicalToHistoryLine(unsigned int logicalLine);
		static void ClearLogicalLine(unsigned int logicalLine);
		static void SyncCursorByView();
		static void RenderViewport();

	public:
		static unsigned int		m_Row;
	static unsigned int		m_Column;

private:
	static unsigned short*	m_VideoMemory;
	/* Debug输出开关 */
	static bool trace_on;
	static char m_History[HISTORY_LINE_COUNT][COLUMNS];
	static unsigned int m_TotalLines;
	static unsigned int m_CurrentLine;
	static unsigned int m_ViewTopLine;
	static bool m_IsBrowsing;
	static bool m_HistoryReady;
};

#endif
