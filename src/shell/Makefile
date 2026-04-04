#
# Makefile for Unix V6++ Shell
#

#命令定义
CC = gcc
LD = ld

#参数定义
CFLAGS = -W -O0 -g -std=gnu89 -m32 -march=i386 -mtune=i386 -mno-mmx -mno-sse -mno-sse2 -nostdlib -nostdinc -nostartfiles -fno-builtin -fcommon -fno-pic -fno-PIE -fno-stack-protector
LDFLAGS = -Wl,-m,elf_i386,-T,../lib/User.ld,-e,main1

#目录定义
TARGET = ./OBJS
V6++LIB = ../lib/Lib_V6++.a
V6++LIB_DEP = ../lib/Lib_V6++.a
USER_LD = ../lib/User.ld
INCLUDE = .
LIB_INCLUDE = ../lib/include

MAKEIMAGEPATH = ../../tools/v6pp-fs-edit-2022/workspace/programs

.PHONY : clean

SHELL_OBJS =$(TARGET)/commandtree.o	\
			$(TARGET)/executecommand.o	\
			$(TARGET)/preexecute.o	\
			$(TARGET)/main.o

SHELL_BIN = $(TARGET)/Shell
WORKSPACE_SHELL = $(MAKEIMAGEPATH)/Shell
			
all	:	$(WORKSPACE_SHELL)

$(TARGET):
	mkdir -p $@

$(MAKEIMAGEPATH):
	mkdir -p $@

$(SHELL_BIN)	:	$(SHELL_OBJS) $(V6++LIB_DEP) $(USER_LD) | $(TARGET)
	$(CC) $(CFLAGS) -no-pie $(SHELL_OBJS) $(V6++LIB) $(LDFLAGS) -o $@


$(WORKSPACE_SHELL)	:	$(SHELL_BIN) | $(MAKEIMAGEPATH)
	cp $< $@
	
$(TARGET)/commandtree.o :	CommandTree.c $(INCLUDE)/CommandTree.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -I"$(LIB_INCLUDE)" -c $< -o $@
	
$(TARGET)/executecommand.o :	ExecuteCommand.c $(INCLUDE)/ExecuteCommand.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -I"$(LIB_INCLUDE)" -c $< -o $@
	
$(TARGET)/preexecute.o :	PreExecute.c $(INCLUDE)/PreExecute.h | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -I"$(LIB_INCLUDE)" -c $< -o $@
	
$(TARGET)/main.o :	main.c | $(TARGET)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -I"$(LIB_INCLUDE)" -c $< -o $@

	
clean:
	rm -f $(TARGET)/*.o $(SHELL_BIN) $(TARGET)/*.exe
	rm -f $(WORKSPACE_SHELL) $(MAKEIMAGEPATH)/Shell.exe
