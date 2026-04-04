if(NOT DEFINED OBJDUMP)
  message(FATAL_ERROR "OBJDUMP is required.")
endif()

if(NOT DEFINED INPUT)
  message(FATAL_ERROR "INPUT is required.")
endif()

if(NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE is required.")
endif()

execute_process(
  COMMAND "${OBJDUMP}" -d "${INPUT}"
  RESULT_VARIABLE OBJDUMP_RESULT
  ERROR_VARIABLE OBJDUMP_ERROR
  OUTPUT_FILE "${OUTPUT_FILE}"
)

if(NOT OBJDUMP_RESULT EQUAL 0)
  message(FATAL_ERROR "objdump failed (${OBJDUMP_RESULT}): ${OBJDUMP_ERROR}")
endif()