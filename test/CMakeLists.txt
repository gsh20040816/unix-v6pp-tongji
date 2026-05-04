add_executable(qemu_stdio_pipe_integration
  qemu_stdio_pipe_integration.cpp
)

target_compile_features(qemu_stdio_pipe_integration PRIVATE cxx_std_17)
target_compile_options(qemu_stdio_pipe_integration PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(qemu_stdio_pipe_integration PRIVATE
  OOS_QEMU_STDIO_IT_ROOT_DIR="${OOS_ROOT_DIR}"
  OOS_QEMU_STDIO_IT_BUILD_DIR="${CMAKE_BINARY_DIR}"
)

set_target_properties(qemu_stdio_pipe_integration PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
)

add_executable(kernel_list_unit
  kernel_list_unit.cpp
)

target_include_directories(kernel_list_unit PRIVATE "${OOS_INCLUDE_DIR}")
target_compile_features(kernel_list_unit PRIVATE cxx_std_17)
target_compile_options(kernel_list_unit PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(kernel_list_unit PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
)

add_executable(qemu_swapstress_integration
  qemu_swapstress_integration.cpp
)

target_compile_features(qemu_swapstress_integration PRIVATE cxx_std_17)
target_compile_options(qemu_swapstress_integration PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(qemu_swapstress_integration PRIVATE
  OOS_QEMU_SWAPSTRESS_IT_ROOT_DIR="${OOS_ROOT_DIR}"
  OOS_QEMU_SWAPSTRESS_IT_BUILD_DIR="${CMAKE_BINARY_DIR}"
)

set_target_properties(qemu_swapstress_integration PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
)

add_test(
  NAME kernel_static_region
  COMMAND "${CMAKE_COMMAND}"
    "-DNM=${OOS_NM_EXECUTABLE}"
    "-DKERNEL_ELF=$<TARGET_FILE:kernel>"
    "-DKERNEL_STATIC_BASE=0xc0100000"
    "-DKERNEL_STATIC_LIMIT=0xc0180000"
    -P "${OOS_CMAKE_DIR}/CheckKernelStaticRegion.cmake"
)

set_tests_properties(kernel_static_region PROPERTIES
  LABELS "unit;kernel;memory"
)

add_test(
  NAME qemu_smoke_boot
  COMMAND "${CMAKE_COMMAND}" -E env
    "OOS_BUILD_DIR=${CMAKE_BINARY_DIR}"
    bash "${OOS_ROOT_DIR}/qemu/smoke_boot.sh"
)

set_tests_properties(qemu_smoke_boot PROPERTIES
  TIMEOUT 120
  LABELS "smoke;qemu;boot"
)

add_test(
  NAME kernel_list_unit
  COMMAND "$<TARGET_FILE:kernel_list_unit>"
)

set_tests_properties(kernel_list_unit PROPERTIES
  LABELS "unit;kernel;list"
)

add_test(
  NAME qemu_stdio_pipe_integration
  COMMAND "${CMAKE_COMMAND}" -E env
    "QEMU_STDIO_PROMPT_TIMEOUT_SEC=90"
    "QEMU_STDIO_CMD_TIMEOUT_SEC=30"
    "QEMU_STDIO_SHUTDOWN_TIMEOUT_SEC=45"
    "OOS_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "$<TARGET_FILE:qemu_stdio_pipe_integration>"
)

set_tests_properties(qemu_stdio_pipe_integration PROPERTIES
  TIMEOUT 180
  LABELS "integration;qemu;stdio"
)

add_test(
  NAME qemu_swapstress_integration
  COMMAND "${CMAKE_COMMAND}" -E env
    "QEMU_SWAPSTRESS_PROMPT_TIMEOUT_SEC=90"
    "QEMU_SWAPSTRESS_CMD_TIMEOUT_SEC=300"
    "QEMU_SWAPSTRESS_SHUTDOWN_TIMEOUT_SEC=45"
    "OOS_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "$<TARGET_FILE:qemu_swapstress_integration>"
)

set_tests_properties(qemu_swapstress_integration PROPERTIES
  TIMEOUT 420
  LABELS "integration;qemu;swap"
)
