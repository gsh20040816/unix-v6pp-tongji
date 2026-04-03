#include "Video.h"

namespace
{
	static const unsigned long DIAGNOSE_VIDEO_MEMORY_BASE = 0xC00B8000;
	static const unsigned int DIAGNOSE_DEFAULT_ROWS = 10;
	typedef __builtin_va_list DiagnoseVaList;

	/*
	 * 内核以 -nostdinc 构建，不能直接包含 <stdarg.h>，
	 * 因此这里使用编译器内建的可变参数支持。
	 */
#define DIAGNOSE_VA_START(ap, last) __builtin_va_start(ap, last)
#define DIAGNOSE_VA_ARG(ap, type) __builtin_va_arg(ap, type)
#define DIAGNOSE_VA_END(ap) __builtin_va_end(ap)

	static inline unsigned short* ResolveDiagnoseVideoMemory(unsigned short*& videoMemory)
	{
		if ( videoMemory == 0 )
		{
			videoMemory = (unsigned short*)DIAGNOSE_VIDEO_MEMORY_BASE;
		}
		return videoMemory;
	}
}

unsigned short* Diagnose::m_VideoMemory = (unsigned short *)(0xB8000 + 0xC0000000);
unsigned int Diagnose::m_Row = 10;
unsigned int Diagnose::m_Column = 0;

unsigned int Diagnose::ROWS = 10;

bool Diagnose::trace_on = true;

Diagnose::Diagnose()
{
	//全部都是static成员变量，所以没有什么需要在构造函数中初始化的。
}

Diagnose::~Diagnose()
{
	//this is an empty dtor
}

void Diagnose::RepairState()
{
	ResolveDiagnoseVideoMemory(Diagnose::m_VideoMemory);

	if ( Diagnose::trace_on )
	{
		if ( Diagnose::ROWS == 0 || Diagnose::ROWS > Diagnose::SCREEN_ROWS )
		{
			Diagnose::ROWS = DIAGNOSE_DEFAULT_ROWS;
		}

		unsigned int firstRow = Diagnose::SCREEN_ROWS - Diagnose::ROWS;
		if ( Diagnose::m_Row < firstRow || Diagnose::m_Row >= Diagnose::SCREEN_ROWS )
		{
			Diagnose::m_Row = firstRow;
		}
	}
	else if ( Diagnose::ROWS > Diagnose::SCREEN_ROWS )
	{
		Diagnose::ROWS = 0;
	}

	if ( Diagnose::m_Column >= Diagnose::COLUMNS )
	{
		Diagnose::m_Column = 0;
	}
}

void Diagnose::TraceOn()
{
	Diagnose::trace_on = 1;
	Diagnose::RepairState();
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

	Diagnose::RepairState();

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
	m_Row += 1;
	m_Column = 0;
}

void Diagnose::WriteChar(const char ch)
{
	Diagnose::RepairState();
	unsigned short* videoMemory = Diagnose::m_VideoMemory;

	if(Diagnose::m_Column >= Diagnose::COLUMNS)
	{
		NextLine();
	}

	if(Diagnose::m_Row >= Diagnose::SCREEN_ROWS)
	{
		Diagnose::ClearScreen();
	}

	videoMemory[Diagnose::m_Row * COLUMNS + Diagnose::m_Column] = (unsigned char) ch | Diagnose::COLOR;
	Diagnose::m_Column++;
}

void Diagnose::ClearScreen()
{
	Diagnose::RepairState();
	unsigned short* videoMemory = Diagnose::m_VideoMemory;
	unsigned int i;

	Diagnose::m_Row = Diagnose::SCREEN_ROWS - Diagnose::ROWS;
	Diagnose::m_Column = 0;

	for(i = 0; i < (COLUMNS * ROWS); i++)
	{
		videoMemory[i + m_Row * COLUMNS] = (unsigned char) ' ' | Diagnose::COLOR;
	}
}
