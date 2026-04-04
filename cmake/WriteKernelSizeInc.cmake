if(NOT DEFINED KERNEL_BIN)
  message(FATAL_ERROR "KERNEL_BIN is required.")
endif()

if(NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE is required.")
endif()

if(NOT EXISTS "${KERNEL_BIN}")
  message(FATAL_ERROR "Kernel binary not found: ${KERNEL_BIN}")
endif()

file(SIZE "${KERNEL_BIN}" KERNEL_SIZE_BYTES)
math(EXPR KERNEL_SECTORS "(${KERNEL_SIZE_BYTES} + 511) / 512")
set(KERNEL_SIZE_LINE "KERNEL_SIZE equ ${KERNEL_SECTORS}\n")

if(EXISTS "${OUTPUT_FILE}")
  file(READ "${OUTPUT_FILE}" OLD_CONTENT)
  if(OLD_CONTENT STREQUAL KERNEL_SIZE_LINE)
    return()
  endif()
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(WRITE "${OUTPUT_FILE}" "${KERNEL_SIZE_LINE}")