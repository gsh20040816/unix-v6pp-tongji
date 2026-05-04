foreach(REQUIRED_VAR NM KERNEL_ELF KERNEL_STATIC_BASE KERNEL_STATIC_LIMIT)
  if(NOT DEFINED ${REQUIRED_VAR})
    message(FATAL_ERROR "${REQUIRED_VAR} is required.")
  endif()
endforeach()

if(NOT EXISTS "${KERNEL_ELF}")
  message(FATAL_ERROR "Kernel ELF not found: ${KERNEL_ELF}")
endif()

execute_process(
  COMMAND "${NM}" -n "${KERNEL_ELF}"
  OUTPUT_VARIABLE NM_OUTPUT
  ERROR_VARIABLE NM_ERROR
  RESULT_VARIABLE NM_RESULT
)

if(NOT NM_RESULT EQUAL 0)
  message(FATAL_ERROR "nm failed (${NM_RESULT}): ${NM_ERROR}")
endif()

string(REGEX MATCH
  "(^|\n)([0-9A-Fa-f]+)[ \t]+[A-Za-z][ \t]+__end(\r?\n|$)"
  END_MATCH
  "${NM_OUTPUT}"
)

if(NOT END_MATCH)
  message(FATAL_ERROR "Kernel symbol __end was not found in ${KERNEL_ELF}")
endif()

set(KERNEL_END_HEX "${CMAKE_MATCH_2}")
math(EXPR KERNEL_END_ADDR "0x${KERNEL_END_HEX}")
math(EXPR STATIC_BASE_ADDR "${KERNEL_STATIC_BASE}")
math(EXPR STATIC_LIMIT_ADDR "${KERNEL_STATIC_LIMIT}")
math(EXPR STATIC_USED_BYTES "${KERNEL_END_ADDR} - ${STATIC_BASE_ADDR}")
math(EXPR STATIC_LIMIT_BYTES "${STATIC_LIMIT_ADDR} - ${STATIC_BASE_ADDR}")
math(EXPR STATIC_FREE_BYTES "${STATIC_LIMIT_ADDR} - ${KERNEL_END_ADDR}")

if(KERNEL_END_ADDR GREATER STATIC_LIMIT_ADDR)
  message(FATAL_ERROR
    "Kernel static region overflow: __end=0x${KERNEL_END_HEX}, "
    "limit=${KERNEL_STATIC_LIMIT}, used=${STATIC_USED_BYTES}, "
    "capacity=${STATIC_LIMIT_BYTES}"
  )
endif()

message(STATUS
  "Kernel static region OK: __end=0x${KERNEL_END_HEX}, "
  "used=${STATIC_USED_BYTES}, free=${STATIC_FREE_BYTES}, "
  "limit=${KERNEL_STATIC_LIMIT}"
)
