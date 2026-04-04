#命令定义
CC = g++
LD = ld
OBJCOPY = objcopy
OBJDUMP = objdump
ASM = nasm
NM = nm
MAKE = make

#参数定义
#g++参数
COMMON_FLAGS = -m32 -nostartfiles -nostdlib -fno-builtin -fno-rtti -fno-exceptions -nostdinc \
	-fno-pic -fno-PIE -fno-stack-protector -fno-use-cxa-atexit -fno-sized-deallocation
CFLAGS = -Wall -O0 -g3 -gdwarf-2 -std=gnu++98 $(COMMON_FLAGS)
#ld参数
LDFLAGS = -m elf_i386 -T $(LINKSCRIPT)

#目录定义
OBJFILES = ../objs
TARGET = ../targets/objs
DIRS = boot kernel machine interrupt mm proc dev fs tty pe lib shell program
INCLUDE = ../include
IMGTARGET = ../targets/img
#"++"号是特殊符号，因此需要加引号
UNIXV6PPTARGET = ../targets/UNIXV6++
V6PP_FS_EDIT_WORKSPACE := ../tools/v6pp-fs-edit-2022/workspace

#文件定义
LINKSCRIPT = Link.ld
SECTOR2_OBJ = sector2.bin
CPP_OBJS = $(wildcard $(TARGET)/*.o)
OBJS = $(TARGET)/$(SECTOR2_OBJ) $(CPP_OBJS)
IMG = c.img

#编译规则
.c.s:
	$(CC) $(CFLAGS) \
	-S -o $*.s $<
.s.o:
	$(AS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<