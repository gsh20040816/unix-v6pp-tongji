#include "Text.h"
#include "Kernel.h"

Text::Text()
{
	//nothing to do here
}

Text::~Text()
{
	//nothing to do here
}

void Text::XccDec()
{
	if ( this->x_ccount == 0 )
		return;

	/* 如果x_ccount递减至0，则释放该共享正文段占据的内存。*/
	if ( --this->x_ccount == 0 )
	{
		Kernel::Instance().GetUserPageManager().FreeMemory(this->x_size, this->x_caddr);
	}
}

void Text::XFree()
{
	this->XccDec();
	/* 非交换模式下，x_daddr不再使用。 */
	if ( --this->x_count == 0 )
	{
		Kernel::Instance().GetFileManager().m_InodeTable->IPut(this->x_iptr);
		this->x_iptr = NULL;
	}
}
