#ifndef TEXT_H
#define TEXT_H

#include "INode.h"

/*
 * @comment 对应Unixv6中 struct text结构
 * 描述可执行代码正文段(code segment)的信息
 *
 *
 */
class Text
{
public:
	static const unsigned int MAX_TEXT_PAGE_COUNT = 10;
	static const unsigned int MAX_RODATA_PAGE_COUNT = 10;

	Text();
	~Text();

	/* 递减x_ccount的值，如果x_ccount递减至0，
	 * 表示内存中已经没有引用该共享正文段的进程，
	 * 则释放该共享正文段占据的内存。
	 */
	void XccDec();

	/*
	 * 进程释放其引用的共享正文段，通常发生在Exit()或Exec()时候。
	 */
	void XFree();

public:
	int				x_daddr;	/* 兼容旧结构保留字段，非交换模式不使用 */
	unsigned long	x_addr[MAX_TEXT_PAGE_COUNT];	/* 代码正文段每一页的物理地址，以字节为单位 */
	unsigned long	x_roaddr[MAX_RODATA_PAGE_COUNT];	/* 只读数据段共享页物理地址，以字节为单位 */
	unsigned int	x_size;		/* 代码段长度，以字节为单位 */
	unsigned int	x_rosize;	/* 可共享只读数据段长度，以字节为单位 */
	unsigned long	x_fileoff;	/* 代码段在可执行文件中的起始偏移 */
	unsigned long	x_filesz;	/* 代码段可从文件回填的有效字节数 */
	unsigned long	x_rofileoff;	/* 只读段在可执行文件中的起始偏移 */
	unsigned long	x_rofilesz;	/* 只读段可从文件回填的有效字节数 */
	Inode*			x_iptr;		/* 内存inode地址 */
	unsigned short	x_count;	/* 共享正文段的进程数 */
	unsigned short	x_ccount;	/* 共享该正文段且图像在内存的进程数 */	
};

#endif

