set(OOS_SHELL_COMPILE_OPTIONS
  -W
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

set(OOS_USER_LINKER_SCRIPT "${OOS_SRC_DIR}/lib/User.ld")

set(OOS_SHELL_LINK_OPTIONS
  -m32
  -nostdlib
  -nostartfiles
  -fno-pic
  -fno-PIE
  -fno-stack-protector
  -no-pie
  "-Wl,--build-id=none"
  "-Wl,-m,elf_i386"
  "-Wl,-T,${OOS_USER_LINKER_SCRIPT}"
  "-Wl,-e,main1"
)

add_executable(oos_shell
  CommandTree.c
  ExecuteCommand.c
  PreExecute.c
  main.c
)

target_compile_options(oos_shell PRIVATE ${OOS_SHELL_COMPILE_OPTIONS})

target_include_directories(oos_shell PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${OOS_SRC_DIR}/lib/include"
)

target_link_options(oos_shell PRIVATE ${OOS_SHELL_LINK_OPTIONS})
target_link_libraries(oos_shell PRIVATE oos_userlib)

set_target_properties(oos_shell PROPERTIES
  OUTPUT_NAME Shell
  RUNTIME_OUTPUT_DIRECTORY "${OOS_FS_PROGRAMS_DIR}"
)

set_property(GLOBAL APPEND PROPERTY OOS_USER_PAYLOAD_TARGETS oos_shell)