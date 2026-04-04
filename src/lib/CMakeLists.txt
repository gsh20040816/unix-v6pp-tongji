set(OOS_USERLIB_COMPILE_OPTIONS
  -w
  -O0
  -g
  -std=gnu89
  -m32
  -march=i386
  -mtune=i386
  -mno-mmx
  -mno-sse
  -mno-sse2
  -nostdlib
  -nostdinc
  -nostartfiles
  -fno-builtin
  -fcommon
  -fno-pic
  -fno-PIE
  -fno-stack-protector
)

add_library(oos_userlib STATIC
  src/file.c
  src/sys.c
  src/stdio.c
  src/stdlib.c
  src/string.c
  src/double.c
  src/float.c
  src/sprintf.c
  src/print_parse.c
  src/time.c
  src/malloc.c
)

target_compile_options(oos_userlib PRIVATE ${OOS_USERLIB_COMPILE_OPTIONS})

target_include_directories(oos_userlib
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set_target_properties(oos_userlib PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/objs"
)