#
# Makefile for all Unix V6++ programs
#

# 默认构建目标。
.DEFAULT_GOAL := all

#命令定义
CC = gcc
LD = ld
OBJDUMP = objdump

#参数定义-nostartfiles
CFLAGS = -w -O0 -g -nostdlib -nostartfiles -nostdinc -fno-builtin -fno-exceptions

#目录定义
TARGET = .\objs
V6++LIB = "..\lib\Lib_V6++.a"
V6++LIB_DEP = ..\lib\Lib_V6++.a
INCLUDE = .
LIB_INCLUDE = ..\lib\include

#MAKEIMAGEPATH = ..\..\tools\MakeImage\bin\Debug\programs
MAKEIMAGEPATH = ..\..\tools\v6pp-fs-edit-2022\workspace\programs
BIN = bin

.PHONY : all build clean

PROGRAM_WORKSPACE_TARGETS = \
	$(MAKEIMAGEPATH)\$(BIN)\cat \
	$(MAKEIMAGEPATH)\$(BIN)\cp \
	$(MAKEIMAGEPATH)\$(BIN)\ls \
	$(MAKEIMAGEPATH)\$(BIN)\mkdir \
	$(MAKEIMAGEPATH)\$(BIN)\rm \
	$(MAKEIMAGEPATH)\$(BIN)\perf \
	$(MAKEIMAGEPATH)\$(BIN)\sig \
	$(MAKEIMAGEPATH)\$(BIN)\copyfile \
	$(MAKEIMAGEPATH)\$(BIN)\shutdown \
	$(MAKEIMAGEPATH)\$(BIN)\test \
	$(MAKEIMAGEPATH)\$(BIN)\forks \
	$(MAKEIMAGEPATH)\$(BIN)\trace \
	$(MAKEIMAGEPATH)\$(BIN)\echo \
	$(MAKEIMAGEPATH)\$(BIN)\date \
	$(MAKEIMAGEPATH)\$(BIN)\newsig \
	$(MAKEIMAGEPATH)\$(BIN)\sigTest \
	$(MAKEIMAGEPATH)\$(BIN)\stack \
	$(MAKEIMAGEPATH)\$(BIN)\malloc \
	$(MAKEIMAGEPATH)\$(BIN)\testSTDOUT \
	$(MAKEIMAGEPATH)\$(BIN)\fork \
	$(MAKEIMAGEPATH)\$(BIN)\HelloWorld \
	$(MAKEIMAGEPATH)\$(BIN)\showStack

all	:	$(PROGRAM_WORKSPACE_TARGETS)

build : $(PROGRAM_WORKSPACE_TARGETS)

define BUILD_PROGRAM
$(TARGET)\$(1).exe : $(2) $(V6++LIB_DEP)
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -I"$(LIB_INCLUDE)" $$(filter %.c,$$^) -e _main1 $(V6++LIB) -o $$@

$(MAKEIMAGEPATH)\$(BIN)\$(1) : $(TARGET)\$(1).exe
	copy $$< $$@
endef

$(eval $(call BUILD_PROGRAM,cat,cat.c))
$(eval $(call BUILD_PROGRAM,cp,cp.c))
$(eval $(call BUILD_PROGRAM,ls,ls.c))
$(eval $(call BUILD_PROGRAM,mkdir,mkdir.c))
$(eval $(call BUILD_PROGRAM,rm,rm.c))
$(eval $(call BUILD_PROGRAM,perf,performance.c))
$(eval $(call BUILD_PROGRAM,sig,sig.c))
$(eval $(call BUILD_PROGRAM,copyfile,copyfile.c))
$(eval $(call BUILD_PROGRAM,shutdown,shutdown.c))
$(eval $(call BUILD_PROGRAM,test,test.c))
$(eval $(call BUILD_PROGRAM,forks,forks.c))
$(eval $(call BUILD_PROGRAM,trace,trace.c))
$(eval $(call BUILD_PROGRAM,echo,echo.c))
$(eval $(call BUILD_PROGRAM,date,date.c))
$(eval $(call BUILD_PROGRAM,newsig,newsig.c))
$(eval $(call BUILD_PROGRAM,sigTest,sigTest.c))
$(eval $(call BUILD_PROGRAM,stack,stack.c))
$(eval $(call BUILD_PROGRAM,malloc,malloc.c))
$(eval $(call BUILD_PROGRAM,testSTDOUT,testSTDOUT.c))
$(eval $(call BUILD_PROGRAM,fork,fork.c))
$(eval $(call BUILD_PROGRAM,HelloWorld,HelloWorld.c))
$(eval $(call BUILD_PROGRAM,showStack,showStack.c))

clean:
	del $(TARGET)\*.exe
	del /Q $(MAKEIMAGEPATH)\$(BIN)\*
