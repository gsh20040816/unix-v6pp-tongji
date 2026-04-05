set(OOS_PROGRAM_COMPILE_OPTIONS
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
  -nostartfiles
  -nostdinc
  -fno-builtin
  -fcommon
  -fno-pic
  -fno-PIE
  -fno-stack-protector
)

set(OOS_USER_LINKER_SCRIPT "${OOS_SRC_DIR}/lib/User.ld")

set(OOS_PROGRAM_LINK_OPTIONS
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

function(oos_add_program OUTPUT_NAME SOURCE_FILE)
  set(TARGET_NAME "oos_prog_${OUTPUT_NAME}")

  add_executable("${TARGET_NAME}" "${SOURCE_FILE}")
  target_compile_options("${TARGET_NAME}" PRIVATE ${OOS_PROGRAM_COMPILE_OPTIONS})

  target_include_directories("${TARGET_NAME}" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${OOS_SRC_DIR}/lib/include"
  )

  target_link_options("${TARGET_NAME}" PRIVATE ${OOS_PROGRAM_LINK_OPTIONS})
  target_link_libraries("${TARGET_NAME}" PRIVATE oos_userlib)

  set_target_properties("${TARGET_NAME}" PROPERTIES
    OUTPUT_NAME "${OUTPUT_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${OOS_FS_PROGRAMS_BIN_DIR}"
  )

  set_property(GLOBAL APPEND PROPERTY OOS_USER_PAYLOAD_TARGETS "${TARGET_NAME}")
endfunction()

oos_add_program(cat cat.c)
oos_add_program(cp cp.c)
oos_add_program(ls ls.c)
oos_add_program(mkdir mkdir.c)
oos_add_program(rm rm.c)
oos_add_program(perf performance.c)
oos_add_program(sig sig.c)
oos_add_program(copyfile copyfile.c)
oos_add_program(shutdown shutdown.c)
oos_add_program(utest test.c)
oos_add_program(forks forks.c)
oos_add_program(trace trace.c)
oos_add_program(echo echo.c)
oos_add_program(date date.c)
oos_add_program(newsig newsig.c)
oos_add_program(sigTest sigTest.c)
oos_add_program(stack stack.c)
oos_add_program(malloc malloc.c)
oos_add_program(testSTDOUT testSTDOUT.c)
oos_add_program(fork fork.c)
oos_add_program(HelloWorld HelloWorld.c)
oos_add_program(showStack showStack.c)
oos_add_program(testfork testfork.c)
oos_add_program(stackpt stackpt.c)
oos_add_program(top top.c)
oos_add_program(argvdump argvdump.c)
oos_add_program(zeropage zeropage.c)