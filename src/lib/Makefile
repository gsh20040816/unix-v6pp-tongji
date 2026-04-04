#
# Makefile for Unix V6++ Mini Clib.
#

#命令定义
CC = gcc
LD = ld
AR = ar

#参数定义
CFLAGS = -w -O0 -g -std=gnu89 -m32 -nostdlib -nostdinc -nostartfiles -fno-builtin -fcommon -fno-pic -fno-PIE -fno-stack-protector -I"$(SRC)"

#目录定义
TARGET = ./objs
INCLUDE = ./include
SRC = ./src

.PHONY : clean

LIB_TARGET = Lib_V6++.a

LIB_OBJS =	$(TARGET)/file.o	\
			$(TARGET)/sys.o		\
			$(TARGET)/stdio.o	\
			$(TARGET)/stdlib.o	\
			$(TARGET)/string.o	\
			$(TARGET)/double.o	\
			$(TARGET)/float.o	\
			$(TARGET)/sprintf.o	\
			$(TARGET)/print_parse.o	\
			$(TARGET)/time.o \
			$(TARGET)/malloc.o

all	:	$(LIB_TARGET)

$(LIB_TARGET)	:	$(LIB_OBJS)
	$(AR) -crs $@ $(LIB_OBJS)

$(TARGET):
	mkdir -p $@

$(TARGET)/file.o :	$(SRC)/file.c $(INCLUDE)/file.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/sys.o	:	$(SRC)/sys.c $(INCLUDE)/sys.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/stdio.o	:	$(SRC)/stdio.c $(INCLUDE)/stdio.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/stdlib.o	:	$(SRC)/stdlib.c $(INCLUDE)/stdlib.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/string.o	:	$(SRC)/string.c $(INCLUDE)/string.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/double.o	:	$(SRC)/double.c $(SRC)/double.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/float.o	:	$(SRC)/float.c $(SRC)/float.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/sprintf.o	:	$(SRC)/sprintf.c | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/print_parse.o	:	$(SRC)/print_parse.c $(SRC)/print_parse.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/time.o	:	$(SRC)/time.c $(INCLUDE)/time.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)/malloc.o	:	$(SRC)/malloc.c $(INCLUDE)/malloc.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

clean:
	rm -f $(TARGET)/*.o $(LIB_TARGET)
	
	
#默认编译规则
.c.s:
	$(CC) $(CFLAGS) \
	-S -o $*.s $<
.s.o:
	$(AS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<
