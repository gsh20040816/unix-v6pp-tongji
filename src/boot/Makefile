include ..\Makefile.inc

TARGET = ..\..\targets\objs
KERNEL_BIN = ..\..\targets\objs\kernel.bin
KERNEL_SIZE_INC = kernel_size.inc

all		:	$(TARGET)\sector2.bin $(TARGET)\support.o
		
$(KERNEL_SIZE_INC) : $(KERNEL_BIN) ..\..\tools\gen_kernel_size.bat
	call ..\..\tools\gen_kernel_size.bat $(KERNEL_BIN) $(KERNEL_SIZE_INC)

$(TARGET)\boot.bin	:	boot.s $(KERNEL_SIZE_INC)
	$(ASM) -f bin boot.s -o $(TARGET)\boot.bin

$(TARGET)\sector2.bin : sector2.s
	$(ASM) -f elf sector2.s -o $(TARGET)\sector2.bin	
	
$(TARGET)\support.o : support.c
	$(CC) $(CFLAGS) -c $< -o $@
