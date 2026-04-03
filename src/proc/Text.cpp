#include "Text.h"
#include "Kernel.h"
#include "PageManager.h"

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
		if ( this->x_caddr != 0 && this->x_size != 0 )
		{
			unsigned int pageCount =
				(this->x_size + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
			for ( unsigned int i = 0; i < pageCount; ++i )
			{
				Kernel::Instance().GetUserPageManager().FreeMemory(PageManager::PAGE_SIZE,
					this->x_caddr + i * PageManager::PAGE_SIZE);
			}
		}
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
