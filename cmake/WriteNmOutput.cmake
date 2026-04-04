if(NOT DEFINED NM)
  message(FATAL_ERROR "NM is required.")
endif()

if(NOT DEFINED INPUT)
  message(FATAL_ERROR "INPUT is required.")
endif()

if(NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE is required.")
endif()

execute_process(
  COMMAND "${NM}" "${INPUT}"
  RESULT_VARIABLE NM_RESULT
  ERROR_VARIABLE NM_ERROR
  OUTPUT_FILE "${OUTPUT_FILE}"
)

if(NOT NM_RESULT EQUAL 0)
  message(FATAL_ERROR "nm failed (${NM_RESULT}): ${NM_ERROR}")
endif()