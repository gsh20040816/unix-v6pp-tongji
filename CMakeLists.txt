cmake_minimum_required(VERSION 3.20)

project(oos LANGUAGES C CXX ASM_NASM)

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
set(OOS_FS_WORKSPACE "${OOS_FS_ROOT}/workspace")
set(OOS_FS_PROGRAMS_DIR "${OOS_FS_WORKSPACE}/programs")
set(OOS_FS_PROGRAMS_BIN_DIR "${OOS_FS_PROGRAMS_DIR}/bin")

set(OOS_BOCHS_BXSHARE "/usr/share/bochs" CACHE PATH "Path to Bochs BIOS/share directory")

find_program(OOS_OBJCOPY_EXECUTABLE NAMES objcopy REQUIRED)
find_program(OOS_OBJDUMP_EXECUTABLE NAMES objdump REQUIRED)
find_program(OOS_NM_EXECUTABLE NAMES nm REQUIRED)
find_program(OOS_BOCHS_EXECUTABLE NAMES bochs)
find_program(OOS_BOCHS_GDB_EXECUTABLE NAMES bochs-gdb)
find_program(OOS_QEMU_EXECUTABLE NAMES qemu-system-i386)

add_subdirectory(tools/v6pp-fs-edit-2022)
add_subdirectory(src)

if(OOS_BOCHS_GDB_EXECUTABLE)
  add_custom_target(bochs
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_BOCHS_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${OOS_BOCHS_LOCK_FILE}"
    COMMAND "${CMAKE_COMMAND}" -E env "BXSHARE=${OOS_BOCHS_BXSHARE}"
            "${OOS_BOCHS_GDB_EXECUTABLE}" -q -f bochsrc_nodebug.bxrc
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
    COMMAND "${CMAKE_COMMAND}" -E env "BXSHARE=${OOS_BOCHS_BXSHARE}"
            "${OOS_BOCHS_GDB_EXECUTABLE}" -q -f bochsrc.bxrc
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
    COMMAND "${CMAKE_COMMAND}" -E env "BXSHARE=${OOS_BOCHS_BXSHARE}"
            "${OOS_BOCHS_GDB_EXECUTABLE}" -q -f bochsrc_gui.bxrc
    WORKING_DIRECTORY "${OOS_BOCHS_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )
else()
  message(STATUS "bochs-gdb executable not found; target 'bochsrc_gui' is not generated.")
endif()

if(OOS_QEMU_EXECUTABLE)
  add_custom_target(qemu
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_QEMU_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E env "QEMU_BIN=${OOS_QEMU_EXECUTABLE}"
            bash "${OOS_QEMU_DIR}/qemu_curses.sh"
    WORKING_DIRECTORY "${OOS_ROOT_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )

  add_custom_target(qemu-gui
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_QEMU_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E env "QEMU_BIN=${OOS_QEMU_EXECUTABLE}"
            bash "${OOS_QEMU_DIR}/qemu_gui.sh"
    WORKING_DIRECTORY "${OOS_ROOT_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )

  add_custom_target(qemu-gdb
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_QEMU_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E env "QEMU_BIN=${OOS_QEMU_EXECUTABLE}"
            bash "${OOS_QEMU_DIR}/qemu_gdb_curses.sh"
    WORKING_DIRECTORY "${OOS_ROOT_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )
else()
  message(STATUS "qemu-system-i386 executable not found; targets 'qemu', 'qemu-gui' and 'qemu-gdb' are not generated.")
endif()