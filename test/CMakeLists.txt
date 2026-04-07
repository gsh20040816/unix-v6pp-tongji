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
