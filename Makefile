SHELL := /bin/bash

.DEFAULT_GOAL := all

.PHONY: all build image clean bochs bochs-gdb fs-tools

SRC_DIR := src
BOOT_DIR := $(SRC_DIR)/boot
TARGET_DIR := targets/objs
UNIXV6PP_DIR := targets/UNIXV6++

KERNEL_EXE := $(TARGET_DIR)/kernel.exe
KERNEL_BIN := $(TARGET_DIR)/kernel.bin
BOOT_BIN := $(TARGET_DIR)/boot.bin
KERNEL_SIZE_INC := $(BOOT_DIR)/kernel_size.inc
IMG_FILE := $(UNIXV6PP_DIR)/c.img

FS_ROOT := tools/v6pp-fs-edit-2022
FS_BUILD_DIR := .build-cache/v6pp-fs-edit-2022-cmake
FS_WORKSPACE := $(FS_ROOT)/workspace
FS_BIN_DIR := $(FS_WORKSPACE)/linux-bin
FILESCANNER := $(FS_BIN_DIR)/filescanner
FSEDIT := $(FS_BIN_DIR)/fsedit

BOCHS_BXSHARE ?= /usr/share/bochs

all: image

build:
	@echo "Building kernel and user programs"
	$(MAKE) -C $(SRC_DIR) build

$(FILESCANNER) $(FSEDIT):
	@echo "Configuring Linux fs tools with CMake"
	mkdir -p $(FS_BUILD_DIR)
	cmake -S $(FS_ROOT) -B $(FS_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	@echo "Building Linux filescanner/fsedit with CMake"
	cmake --build $(FS_BUILD_DIR) --target filescanner fsedit --parallel

fs-tools: $(FILESCANNER) $(FSEDIT)

image: build fs-tools
	@echo "Exporting kernel.bin from latest kernel.exe"
	objcopy -O binary $(KERNEL_EXE) $(KERNEL_BIN)
	@kernel_size="$$(stat -c '%s' $(KERNEL_BIN))"; \
	kernel_sectors="$$(( (kernel_size + 511) / 512 ))"; \
	kernel_size_line="KERNEL_SIZE equ $$kernel_sectors"; \
	existing_line=""; \
	if [[ -f $(KERNEL_SIZE_INC) ]]; then \
		existing_line="$$(sed -e 's/\r$$//' $(KERNEL_SIZE_INC) | head -n 1)"; \
	fi; \
	if [[ "$$existing_line" != "$$kernel_size_line" ]]; then \
		printf '%s\n' "$$kernel_size_line" > $(KERNEL_SIZE_INC); \
	fi; \
	echo "Building boot.bin with KERNEL_SIZE=$$kernel_sectors"
	$(MAKE) -C $(BOOT_DIR) ../../$(BOOT_BIN)
	mkdir -p $(FS_WORKSPACE)
	cp $(BOOT_BIN) $(FS_WORKSPACE)/boot.bin
	cp $(KERNEL_BIN) $(FS_WORKSPACE)/kernel.bin
	@echo "Generating disk image on Linux with v6pp-fs-edit-2022"
	cd $(FS_WORKSPACE) && ./linux-bin/filescanner | ./linux-bin/fsedit c.img c
	mkdir -p $(UNIXV6PP_DIR)
	cp $(FS_WORKSPACE)/c.img $(IMG_FILE)

clean:
	@echo "Cleaning local build artifacts"
	-$(MAKE) -C $(SRC_DIR) clean
	@echo "Cleaning disk images on Linux"
	rm -f $(FS_WORKSPACE)/c.img $(IMG_FILE)

bochs: all
	@echo "Starting bochs in $(UNIXV6PP_DIR)"
	cd $(UNIXV6PP_DIR) && BXSHARE="$${OOS_LINUX_BXSHARE:-$(BOCHS_BXSHARE)}" bochs -q -f bochsrc_gui.bxrc

bochs-gdb: all
	@echo "Starting bochs-gdb in $(UNIXV6PP_DIR)"
	cd $(UNIXV6PP_DIR) && BXSHARE="$${OOS_LINUX_BXSHARE:-$(BOCHS_BXSHARE)}" bochs-gdb -q -f bochsrc.bxrc
