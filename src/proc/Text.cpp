#include "Text.h"
#include "Kernel.h"
#include "PageManager.h"
#include "Utility.h"

Text::Text()
{
	this->Reset();
}

Text::~Text()
{
	//nothing to do here
}

void Text::Reset()
{
	this->x_daddr = 0;
	this->x_size = 0;
	this->x_rosize = 0;
	this->x_fileoff = 0;
	this->x_filesz = 0;
	this->x_rofileoff = 0;
	this->x_rofilesz = 0;
	this->x_iptr = NULL;
	this->x_count = 0;
	this->x_ccount = 0;
	this->x_addr.release();
	this->x_roaddr.release();
}

bool Text::ResizePageVectors(unsigned int textPageCount, unsigned int rodataPageCount)
{
	if ( this->x_addr.resize(textPageCount) == false )
	{
		this->x_addr.release();
		this->x_roaddr.release();
		return false;
	}

	if ( this->x_roaddr.resize(rodataPageCount) == false )
	{
		this->x_addr.release();
		this->x_roaddr.release();
		return false;
	}

	this->ClearPageVectors();
	return true;
}

void Text::ClearPageVectors()
{
	for ( unsigned int i = 0; i < this->x_addr.size(); ++i )
	{
		this->x_addr[i] = 0;
	}
	for ( unsigned int i = 0; i < this->x_roaddr.size(); ++i )
	{
		this->x_roaddr[i] = 0;
	}
}

void Text::XccDec()
{
	if ( this->x_ccount == 0 )
		return;

	/* 如果x_ccount递减至0，则释放该共享正文段占据的内存。*/
	if ( --this->x_ccount == 0 )
	{
		if ( this->x_size != 0 )
		{
			unsigned int pageCount =
				(this->x_size + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
			if ( pageCount > this->x_addr.size() )
			{
				Utility::Panic("Text page count overflow");
			}

			for ( unsigned int i = 0; i < pageCount; ++i )
			{
				if ( this->x_addr[i] == 0 )
				{
					continue;
				}

				Kernel::Instance().GetUserPageManager().FreePage(this->x_addr[i]);
				this->x_addr[i] = 0;
			}
		}

		if ( this->x_rosize != 0 )
		{
			unsigned int roPageCount =
				(this->x_rosize + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
			if ( roPageCount > this->x_roaddr.size() )
			{
				Utility::Panic("Rodata page count overflow");
			}

			for ( unsigned int i = 0; i < roPageCount; ++i )
			{
				if ( this->x_roaddr[i] == 0 )
				{
					continue;
				}

				Kernel::Instance().GetUserPageManager().FreePage(this->x_roaddr[i]);
				this->x_roaddr[i] = 0;
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
		this->Reset();
	}
}
