include Makefile.inc

.PHONY : all build deploy run clean $(DIRS)

KERNEL_EXE = $(TARGET)\kernel.exe
KERNEL_BIN = $(TARGET)\kernel.bin
KERNEL_SYM = $(TARGET)\kernel.sym
KERNEL_ASM = $(TARGET)\kernel.asm

IMG_BOOT = $(IMGTARGET)\boot.bin
IMG_KERNEL_BIN = $(IMGTARGET)\kernel.bin
IMG_KERNEL_SYM = $(IMGTARGET)\kernel.sym
IMG_KERNEL_ASM = $(IMGTARGET)\kernel.asm

WORKSPACE_BOOT = $(V6PP_FS_EDIT_WORKSPACE)\boot.bin
WORKSPACE_KERNEL = $(V6PP_FS_EDIT_WORKSPACE)\kernel.bin
WORKSPACE_IMG = $(V6PP_FS_EDIT_WORKSPACE)\$(IMG)
FINAL_IMG = ..\targets\UNIXV6++\$(IMG)

WORKSPACE_PROGRAMS = \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\Shell.exe \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\cat \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\cp \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\ls \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\mkdir \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\rm \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\perf \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\sig \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\copyfile \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\shutdown \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\test \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\forks \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\trace \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\echo \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\date \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\newsig \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\sigTest \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\stack \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\malloc \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\testSTDOUT \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\fork \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\HelloWorld \
	$(V6PP_FS_EDIT_WORKSPACE)\programs\bin\showStack

all: build deploy 

# .PHONY: qemug
# qemug: all
#	qemu-system-i386 -m 32M -audiodev pa,id=hda -machine pcspk-audiodev=hda -rtc 
# base=localtime \
#	-boot c -drive file=../targets/UNIXV6++/c.img,if=ide,index=0,media=disk,format=raw \
#	-s -S

#.PHONY: qemu
#qemu: all
#	qemu-system-i386 -m 32M -audiodev pa,id=hda -machine pcspk-audiodev=hda -rtc #base=localtime \
#	-boot c -drive file=../targets/UNIXV6++/c.img,if=ide,index=0,media=disk,format=raw 

$(DIRS) : 
	$(MAKE) --directory=$@
	
build	: $(DIRS) $(KERNEL_ASM)

deploy	: $(DIRS) $(IMG_BOOT) $(IMG_KERNEL_BIN) $(IMG_KERNEL_SYM) $(IMG_KERNEL_ASM) $(FINAL_IMG)

$(KERNEL_EXE)	: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(KERNEL_BIN)	: $(KERNEL_EXE)
	$(OBJCOPY) -O binary $< $@

$(KERNEL_SYM)	: $(KERNEL_EXE)
	$(NM) $< > $@

$(KERNEL_ASM)	: $(KERNEL_EXE)
	$(OBJDUMP) -d $< > $@

$(IMG_BOOT)	: $(TARGET)\boot.bin
	copy $< $@

$(IMG_KERNEL_BIN)	: $(KERNEL_BIN)
	copy $< $@

$(IMG_KERNEL_SYM)	: $(KERNEL_SYM)
	copy $< $@

$(IMG_KERNEL_ASM)	: $(KERNEL_ASM)
	copy $< $@

#	cd $(IMGTARGET) && partcopy boot.bin 0 200 c.img 0
#	cd $(IMGTARGET) && partcopy kernel.bin 0 13000 c.img 200

#	copy $(TARGET)\boot.bin $(MakeImageDebug)\boot.bin
#	copy $(TARGET)\kernel.bin $(MakeImageDebug)\kernel.bin
#	copy $(IMGTARGET)\c.img $(MakeImageDebug)\c.img

#	cd $(MakeImageDebug) && build.exe c.img boot.bin kernel.bin programs
#	copy $(MakeImageDebug)\$(IMG) $(UNIXV6PPTARGET)\$(IMG)

$(WORKSPACE_BOOT)	: $(TARGET)\boot.bin
	copy $< $@

$(WORKSPACE_KERNEL)	: $(KERNEL_BIN)
	copy $< $@

$(WORKSPACE_IMG)	: $(WORKSPACE_BOOT) $(WORKSPACE_KERNEL) $(WORKSPACE_PROGRAMS) \
		$(V6PP_FS_EDIT_WORKSPACE)\filescanner.exe $(V6PP_FS_EDIT_WORKSPACE)\fsedit.exe
	cd $(V6PP_FS_EDIT_WORKSPACE) && filescanner.exe | fsedit.exe $(IMG) c

$(FINAL_IMG)	: $(WORKSPACE_IMG)
	copy $< "$@"

clean	:	
	del     $(TARGET)\*.o
	del     $(TARGET)\*.exe
	del     $(TARGET)\*.bin
	del     $(TARGET)\*.sym
	del     $(TARGET)\*.asm
	
	del     $(IMGTARGET)\*.bin
	del     $(IMGTARGET)\*.sym
	del     $(IMGTARGET)\*.asm
	
	del     "$(FINAL_IMG)"
