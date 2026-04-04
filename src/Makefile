include Makefile.inc

.PHONY : all build deploy run clean prepare_dirs $(DIRS)

KERNEL_EXE = $(TARGET)/kernel.exe
KERNEL_BIN = $(TARGET)/kernel.bin
KERNEL_SYM = $(TARGET)/kernel.sym
KERNEL_ASM = $(TARGET)/kernel.asm

IMG_BOOT = $(IMGTARGET)/boot.bin
IMG_KERNEL_BIN = $(IMGTARGET)/kernel.bin
IMG_KERNEL_SYM = $(IMGTARGET)/kernel.sym
IMG_KERNEL_ASM = $(IMGTARGET)/kernel.asm

WORKSPACE_BOOT = $(V6PP_FS_EDIT_WORKSPACE)/boot.bin
WORKSPACE_KERNEL = $(V6PP_FS_EDIT_WORKSPACE)/kernel.bin

all: build

prepare_dirs:
	mkdir -p $(TARGET) $(IMGTARGET)

$(DIRS) :
	$(MAKE) --directory=$@

build	: prepare_dirs $(DIRS) $(KERNEL_ASM)

deploy	: build $(IMG_BOOT) $(IMG_KERNEL_BIN) $(IMG_KERNEL_SYM) $(IMG_KERNEL_ASM) $(WORKSPACE_BOOT) $(WORKSPACE_KERNEL)

$(KERNEL_EXE)	: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# Parallel safety: link must wait until all per-directory sub-makes finish.
$(KERNEL_EXE)	: $(DIRS)

$(KERNEL_BIN)	: $(KERNEL_EXE)
	$(OBJCOPY) -O binary $< $@

$(KERNEL_SYM)	: $(KERNEL_EXE)
	$(NM) $< > $@

$(KERNEL_ASM)	: $(KERNEL_EXE)
	$(OBJDUMP) -d $< > $@

$(TARGET)/$(SECTOR2_OBJ) :
	$(MAKE) --directory=boot ../../targets/objs/$(SECTOR2_OBJ)

$(TARGET)/boot.bin : $(KERNEL_BIN)
	$(MAKE) --directory=boot ../../targets/objs/boot.bin

$(IMG_BOOT)	: $(TARGET)/boot.bin
	cp $< $@

$(IMG_KERNEL_BIN)	: $(KERNEL_BIN)
	cp $< $@

$(IMG_KERNEL_SYM)	: $(KERNEL_SYM)
	cp $< $@

$(IMG_KERNEL_ASM)	: $(KERNEL_ASM)
	cp $< $@

$(WORKSPACE_BOOT)	: $(TARGET)/boot.bin
	mkdir -p $(V6PP_FS_EDIT_WORKSPACE)
	cp $< $@

$(WORKSPACE_KERNEL)	: $(KERNEL_BIN)
	mkdir -p $(V6PP_FS_EDIT_WORKSPACE)
	cp $< $@

clean	:
	-$(MAKE) --directory=lib clean
	-$(MAKE) --directory=shell clean
	-$(MAKE) --directory=program clean
	rm -f $(TARGET)/*.o $(TARGET)/*.exe $(TARGET)/*.bin $(TARGET)/*.sym $(TARGET)/*.asm
	rm -f $(IMGTARGET)/*.bin $(IMGTARGET)/*.sym $(IMGTARGET)/*.asm
	rm -f $(UNIXV6PPTARGET)/$(IMG)
