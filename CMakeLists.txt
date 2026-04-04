cmake_minimum_required(VERSION 3.20)

project(oos LANGUAGES C CXX ASM_NASM)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "OOS CMake build currently supports Linux hosts only.")
endif()

set(OOS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(OOS_CMAKE_DIR "${OOS_ROOT_DIR}/cmake")
set(OOS_SRC_DIR "${OOS_ROOT_DIR}/src")
set(OOS_INCLUDE_DIR "${OOS_SRC_DIR}/include")
set(OOS_TARGET_OBJ_DIR "${OOS_ROOT_DIR}/targets/objs")
set(OOS_UNIXV6PP_DIR "${OOS_ROOT_DIR}/targets/UNIXV6++")
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

add_subdirectory(tools/v6pp-fs-edit-2022)
add_subdirectory(src)

if(OOS_BOCHS_GDB_EXECUTABLE)
  add_custom_target(bochs
    COMMAND "${CMAKE_COMMAND}" -E env "BXSHARE=${OOS_BOCHS_BXSHARE}"
            "${OOS_BOCHS_GDB_EXECUTABLE}" -q -f bochsrc_nodebug.bxrc
    WORKING_DIRECTORY "${OOS_UNIXV6PP_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )
else()
  message(STATUS "bochs-gdb executable not found; target 'bochs' is not generated.")
endif()

if(OOS_BOCHS_GDB_EXECUTABLE)
  add_custom_target(bochs-gdb
    COMMAND "${CMAKE_COMMAND}" -E env "BXSHARE=${OOS_BOCHS_BXSHARE}"
            "${OOS_BOCHS_GDB_EXECUTABLE}" -q -f bochsrc.bxrc
    WORKING_DIRECTORY "${OOS_UNIXV6PP_DIR}"
    DEPENDS image
    USES_TERMINAL
    VERBATIM
  )
else()
  message(STATUS "bochs-gdb executable not found; target 'bochs-gdb' is not generated.")
endif()