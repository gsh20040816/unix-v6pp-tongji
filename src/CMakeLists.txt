set(OOS_KERNEL_COMMON_COMPILE_OPTIONS
  -m32
  -march=i386
  -mtune=i386
  -mno-mmx
  -mno-sse
  -mno-sse2
  -nostartfiles
  -nostdlib
  -nostdinc
  -fno-builtin
  -fno-rtti
  -fno-exceptions
  -fno-pic
  -fno-PIE
  -fno-stack-protector
  -fno-use-cxa-atexit
  -fno-sized-deallocation
  -Wall
  -O0
  -g3
  -gdwarf-2
  -std=gnu++98
)

add_library(oos_kernel_flags INTERFACE)
target_include_directories(oos_kernel_flags INTERFACE "${OOS_INCLUDE_DIR}")
target_compile_options(oos_kernel_flags INTERFACE ${OOS_KERNEL_COMMON_COMPILE_OPTIONS})

add_subdirectory(boot)
add_subdirectory(kernel)
add_subdirectory(machine)
add_subdirectory(interrupt)
add_subdirectory(mm)
add_subdirectory(proc)
add_subdirectory(dev)
add_subdirectory(fs)
add_subdirectory(tty)
add_subdirectory(pe)

if(NOT DEFINED OOS_SECTOR2_OBJECT)
  message(FATAL_ERROR "boot module did not define OOS_SECTOR2_OBJECT")
endif()

add_executable(kernel
  "${OOS_SECTOR2_OBJECT}"
  $<TARGET_OBJECTS:oos_boot_obj>
  $<TARGET_OBJECTS:oos_kernel_obj>
  $<TARGET_OBJECTS:oos_machine_obj>
  $<TARGET_OBJECTS:oos_interrupt_obj>
  $<TARGET_OBJECTS:oos_mm_obj>
  $<TARGET_OBJECTS:oos_proc_obj>
  $<TARGET_OBJECTS:oos_dev_obj>
  $<TARGET_OBJECTS:oos_fs_obj>
  $<TARGET_OBJECTS:oos_tty_obj>
  $<TARGET_OBJECTS:oos_pe_obj>
)

add_dependencies(kernel oos_sector2_obj)

set_target_properties(kernel PROPERTIES
  OUTPUT_NAME kernel
  RUNTIME_OUTPUT_DIRECTORY "${OOS_TARGET_OBJ_DIR}"
)

set(OOS_KERNEL_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/Link.ld")
file(RELATIVE_PATH OOS_KERNEL_LINKER_SCRIPT_REL "${CMAKE_BINARY_DIR}" "${OOS_KERNEL_LINKER_SCRIPT}")

set_property(TARGET kernel APPEND PROPERTY LINK_DEPENDS "${OOS_KERNEL_LINKER_SCRIPT_REL}")

target_link_options(kernel PRIVATE
  -m32
  -nostdlib
  -nostartfiles
  -fno-pic
  -fno-PIE
  -fno-stack-protector
  -no-pie
  "-Wl,-m,elf_i386"
  "-Wl,-T,${OOS_KERNEL_LINKER_SCRIPT_REL}"
)

set(OOS_KERNEL_BIN "${OOS_TARGET_OBJ_DIR}/kernel.bin")
set(OOS_KERNEL_SYM "${OOS_TARGET_OBJ_DIR}/kernel.sym")
set(OOS_KERNEL_ASM "${OOS_TARGET_OBJ_DIR}/kernel.asm")

add_custom_command(
  OUTPUT "${OOS_KERNEL_BIN}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_TARGET_OBJ_DIR}"
  COMMAND "${OOS_OBJCOPY_EXECUTABLE}" -O binary "$<TARGET_FILE:kernel>" "${OOS_KERNEL_BIN}"
  DEPENDS kernel
  VERBATIM
)

add_custom_command(
  OUTPUT "${OOS_KERNEL_SYM}"
  COMMAND "${CMAKE_COMMAND}"
          "-DNM=${OOS_NM_EXECUTABLE}"
          "-DINPUT=$<TARGET_FILE:kernel>"
          "-DOUTPUT_FILE=${OOS_KERNEL_SYM}"
          -P "${OOS_CMAKE_DIR}/WriteNmOutput.cmake"
  DEPENDS kernel
  VERBATIM
)

add_custom_command(
  OUTPUT "${OOS_KERNEL_ASM}"
  COMMAND "${CMAKE_COMMAND}"
          "-DOBJDUMP=${OOS_OBJDUMP_EXECUTABLE}"
          "-DINPUT=$<TARGET_FILE:kernel>"
          "-DOUTPUT_FILE=${OOS_KERNEL_ASM}"
          -P "${OOS_CMAKE_DIR}/WriteObjdumpOutput.cmake"
  DEPENDS kernel
  VERBATIM
)

add_custom_target(oos_kernel_bin DEPENDS "${OOS_KERNEL_BIN}")
add_custom_target(oos_kernel_sym DEPENDS "${OOS_KERNEL_SYM}")
add_custom_target(oos_kernel_asm DEPENDS "${OOS_KERNEL_ASM}")

set(OOS_BOOT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/boot-generated")
set(OOS_KERNEL_SIZE_INC "${OOS_BOOT_GENERATED_DIR}/kernel_size.generated.inc")

add_custom_command(
  OUTPUT "${OOS_KERNEL_SIZE_INC}"
  COMMAND "${CMAKE_COMMAND}"
          "-DKERNEL_BIN=${OOS_KERNEL_BIN}"
          "-DOUTPUT_FILE=${OOS_KERNEL_SIZE_INC}"
          -P "${OOS_CMAKE_DIR}/WriteKernelSizeInc.cmake"
  DEPENDS "${OOS_KERNEL_BIN}"
  VERBATIM
)

add_custom_target(oos_kernel_size_inc DEPENDS "${OOS_KERNEL_SIZE_INC}")

set(OOS_BOOT_BIN "${OOS_TARGET_OBJ_DIR}/boot.bin")

add_custom_command(
  OUTPUT "${OOS_BOOT_BIN}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_TARGET_OBJ_DIR}"
  COMMAND "${CMAKE_ASM_NASM_COMPILER}"
          -f bin
          "-I${OOS_BOOT_GENERATED_DIR}/"
          "${CMAKE_CURRENT_SOURCE_DIR}/boot/boot.s"
          -o "${OOS_BOOT_BIN}"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/boot/boot.s"
    "${OOS_KERNEL_SIZE_INC}"
  VERBATIM
)

add_custom_target(oos_boot_bin DEPENDS "${OOS_BOOT_BIN}")

add_subdirectory(lib)
add_subdirectory(shell)
add_subdirectory(program)

get_property(OOS_USER_PAYLOAD_TARGETS GLOBAL PROPERTY OOS_USER_PAYLOAD_TARGETS)
if(NOT OOS_USER_PAYLOAD_TARGETS)
  message(FATAL_ERROR "No user payload targets were registered by shell/program modules.")
endif()

add_custom_target(oos_user_payload DEPENDS ${OOS_USER_PAYLOAD_TARGETS})
add_custom_target(oos_fs_tools DEPENDS filescanner fsedit)

set(OOS_WORKSPACE_IMG "${OOS_FS_WORKSPACE}/c.img")
set(OOS_FINAL_IMG "${OOS_BOCHS_DIR}/c.img")

add_custom_command(
  OUTPUT "${OOS_FINAL_IMG}"
  BYPRODUCTS "${OOS_WORKSPACE_IMG}"
  COMMAND "${CMAKE_COMMAND}"
          "-DWORKSPACE_DIR=${OOS_FS_WORKSPACE}"
          "-DBOOT_BIN=${OOS_BOOT_BIN}"
          "-DKERNEL_BIN=${OOS_KERNEL_BIN}"
          "-DFILESCANNER=$<TARGET_FILE:filescanner>"
          "-DFSEDIT=$<TARGET_FILE:fsedit>"
          "-DOUTPUT_IMG=${OOS_FINAL_IMG}"
          -P "${OOS_CMAKE_DIR}/GenerateDiskImage.cmake"
  DEPENDS
    oos_boot_bin
    oos_kernel_bin
    oos_user_payload
    oos_fs_tools
  VERBATIM
)

add_custom_target(image ALL DEPENDS "${OOS_FINAL_IMG}")