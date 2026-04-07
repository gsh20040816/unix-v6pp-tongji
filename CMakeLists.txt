cmake_minimum_required(VERSION 3.20)

project(oos LANGUAGES C CXX ASM_NASM)

option(BUILD_TESTING "Build and register CTest tests" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "OOS CMake build currently supports Linux hosts only.")
endif()

set(OOS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(OOS_CMAKE_DIR "${OOS_ROOT_DIR}/cmake")
set(OOS_SRC_DIR "${OOS_ROOT_DIR}/src")
set(OOS_INCLUDE_DIR "${OOS_SRC_DIR}/include")
set(OOS_TARGET_OBJ_DIR "${CMAKE_BINARY_DIR}/objs")
set(OOS_BOCHS_DIR "${OOS_ROOT_DIR}/bochs")
set(OOS_QEMU_DIR "${OOS_ROOT_DIR}/qemu")
set(OOS_BOCHS_LOCK_FILE "${CMAKE_BINARY_DIR}/c.img.lock")
set(OOS_FS_ROOT "${OOS_ROOT_DIR}/tools/v6pp-fs-edit-2022")
set(OOS_FS_CONTENT_DIR "${OOS_FS_ROOT}/fs")
set(OOS_FS_WORKSPACE "${CMAKE_BINARY_DIR}/fs-workspace")
set(OOS_FS_PAYLOAD_DIR "${CMAKE_BINARY_DIR}/fs-payload")
set(OOS_FS_PROGRAMS_DIR "${OOS_FS_PAYLOAD_DIR}/programs")
set(OOS_FS_PROGRAMS_BIN_DIR "${OOS_FS_PROGRAMS_DIR}/bin")
set(OOS_FS_TOOLS_BIN_DIR "${CMAKE_BINARY_DIR}/fs-tools")

# Keep clangd lookup independent from a fixed build directory.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  file(MAKE_DIRECTORY "${OOS_ROOT_DIR}/.vscode")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
      "${CMAKE_BINARY_DIR}/compile_commands.json"
      "${OOS_ROOT_DIR}/.vscode/compile_commands.json"
  )
endif()

set(OOS_BOCHS_BXSHARE "/usr/share/bochs" CACHE PATH "Path to Bochs BIOS/share directory")

find_program(OOS_OBJCOPY_EXECUTABLE NAMES objcopy REQUIRED)
find_program(OOS_OBJDUMP_EXECUTABLE NAMES objdump REQUIRED)
find_program(OOS_NM_EXECUTABLE NAMES nm REQUIRED)
find_program(OOS_BOCHS_GDB_EXECUTABLE NAMES bochs-gdb)
find_program(OOS_QEMU_EXECUTABLE NAMES qemu-system-i386)

option(OOS_KERNEL_DIAGNOSE_TO_DEBUGCON
  "Mirror kernel Diagnose output to QEMU debugcon ports (0xE9 and 0x402)"
  OFF
)

option(OOS_KERNEL_TTY_STDIO_SERIAL
  "Route kernel TTY input/output through COM1 serial port (for QEMU stdio mode)"
  OFF
)

add_subdirectory(tools/v6pp-fs-edit-2022)
add_subdirectory(src)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

set(OOS_BOCHS_CONFIG_DIR "${CMAKE_BINARY_DIR}/bochs-config")
set(OOS_BOCHSRC_TEMPLATE "${OOS_BOCHS_DIR}/bochsrc_template.bxrc.in")
set(OOS_BOCHSRC_NODEBUG "${OOS_BOCHS_CONFIG_DIR}/bochsrc_nodebug.bxrc")
set(OOS_BOCHSRC_GDB "${OOS_BOCHS_CONFIG_DIR}/bochsrc.bxrc")
set(OOS_BOCHSRC_GUI "${OOS_BOCHS_CONFIG_DIR}/bochsrc_gui.bxrc")

function(oos_generate_bochsrc OUTPUT_FILE GDBSTUB_LINE DISPLAY_LIBRARY_LINE)
  set(BOCHSRC_GDBSTUB_LINE "${GDBSTUB_LINE}")
  set(BOCHSRC_DISPLAY_LIBRARY_LINE "${DISPLAY_LIBRARY_LINE}")
  set(BOCHSRC_DISK_IMAGE "${CMAKE_BINARY_DIR}/c.img")
  set(BOCHSRC_LOG_FILE "${CMAKE_BINARY_DIR}/bochsout.txt")
  configure_file("${OOS_BOCHSRC_TEMPLATE}" "${OUTPUT_FILE}" @ONLY)
endfunction()

file(MAKE_DIRECTORY "${OOS_BOCHS_CONFIG_DIR}")
oos_generate_bochsrc("${OOS_BOCHSRC_NODEBUG}"
  "# gdbstub: enabled=1, port=1234, text_base=0, data_base=0, bss_base=0"
  "display_library: term"
)
oos_generate_bochsrc("${OOS_BOCHSRC_GDB}"
  "gdbstub: enabled=1, port=1234, text_base=0, data_base=0, bss_base=0"
  "display_library: term"
)
oos_generate_bochsrc("${OOS_BOCHSRC_GUI}"
  "#gdbstub: enabled=1, port=1234, text_base=0, data_base=0, bss_base=0"
  "#display_library: term"
)

if(OOS_BOCHS_GDB_EXECUTABLE)
  add_custom_target(bochs
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_BOCHS_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${OOS_BOCHS_LOCK_FILE}"
    COMMAND "${CMAKE_COMMAND}" -E env
            "BXSHARE=${OOS_BOCHS_BXSHARE}"
            "BOCHS_BIN=${OOS_BOCHS_GDB_EXECUTABLE}"
          bash "${OOS_BOCHS_DIR}/run_bochs.sh" "${OOS_BOCHSRC_NODEBUG}"
    WORKING_DIRECTORY "${OOS_BOCHS_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )
else()
  message(STATUS "bochs-gdb executable not found; target 'bochs' is not generated.")
endif()

if(OOS_BOCHS_GDB_EXECUTABLE)
  add_custom_target(bochs-gdb
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_BOCHS_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${OOS_BOCHS_LOCK_FILE}"
    COMMAND "${CMAKE_COMMAND}" -E env
            "BXSHARE=${OOS_BOCHS_BXSHARE}"
            "BOCHS_BIN=${OOS_BOCHS_GDB_EXECUTABLE}"
          bash "${OOS_BOCHS_DIR}/run_bochs.sh" "${OOS_BOCHSRC_GDB}"
    WORKING_DIRECTORY "${OOS_BOCHS_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )
else()
  message(STATUS "bochs-gdb executable not found; target 'bochs-gdb' is not generated.")
endif()

if(OOS_BOCHS_GDB_EXECUTABLE)
  add_custom_target(bochsrc_gui
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_BOCHS_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${OOS_BOCHS_LOCK_FILE}"
    COMMAND "${CMAKE_COMMAND}" -E env
            "BXSHARE=${OOS_BOCHS_BXSHARE}"
            "BOCHS_BIN=${OOS_BOCHS_GDB_EXECUTABLE}"
          bash "${OOS_BOCHS_DIR}/run_bochs.sh" "${OOS_BOCHSRC_GUI}"
    WORKING_DIRECTORY "${OOS_BOCHS_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )
else()
  message(STATUS "bochs-gdb executable not found; target 'bochsrc_gui' is not generated.")
endif()

function(add_oos_qemu_target TARGET_NAME MODE)
  add_custom_target(${TARGET_NAME}
    COMMAND "${CMAKE_COMMAND}" -E env
            "QEMU_BIN=${OOS_QEMU_EXECUTABLE}"
            "QEMU_MODE=${MODE}"
            "OOS_BUILD_DIR=${CMAKE_BINARY_DIR}"
            bash "${OOS_QEMU_DIR}/run_qemu.sh"
    WORKING_DIRECTORY "${OOS_ROOT_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )
endfunction()

if(OOS_QEMU_EXECUTABLE)
  add_oos_qemu_target(qemu stdio)
  add_oos_qemu_target(qemu-curses curses)
  add_oos_qemu_target(qemu-gui gui)
  add_oos_qemu_target(qemu-gdb gdb-stdio)
  add_oos_qemu_target(qemu-gdb-curses gdb-curses)
else()
  message(STATUS "qemu-system-i386 executable not found; targets 'qemu', 'qemu-curses', 'qemu-gui', 'qemu-gdb' and 'qemu-gdb-curses' are not generated.")
endif()
