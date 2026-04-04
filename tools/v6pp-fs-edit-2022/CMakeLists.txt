cmake_minimum_required(VERSION 3.16)

project(v6pp_fs_tools LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "v6pp-fs-edit-2022 CMake build supports Linux only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/workspace/linux-bin")

add_executable(filescanner
  src/FileScanner/src/main.cpp
)

add_executable(fsedit
  src/FsEditor/src/entry/main.cpp
  src/FsEditor/src/FileSystemAdapter/FileSystemAdapter.cpp
  src/FsEditor/src/structures/Inode.cpp
  src/FsEditor/src/structures/SuperBlock.cpp
  src/FsEditor/src/structures/InodeDirectory.cpp
)

target_include_directories(fsedit PRIVATE
  src/FsEditor/src/include
)
