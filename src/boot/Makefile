include ../Makefile.inc

TARGET = ../../targets/objs
KERNEL_SIZE_INC = kernel_size.inc

all		:	$(TARGET)/sector2.bin $(TARGET)/support.o

$(TARGET)/boot.bin	:	boot.s $(KERNEL_SIZE_INC)
	$(ASM) -f bin boot.s -o $(TARGET)/boot.bin

$(TARGET)/sector2.bin : sector2.s
	$(ASM) -f elf32 sector2.s -o $(TARGET)/sector2.bin

$(TARGET)/support.o : support.c
	$(CC) $(CFLAGS) -c $< -o $@
