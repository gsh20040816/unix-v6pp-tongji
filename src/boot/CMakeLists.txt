add_library(oos_boot_obj OBJECT support.c)
set_source_files_properties(support.c PROPERTIES LANGUAGE CXX)
target_link_libraries(oos_boot_obj PRIVATE oos_kernel_flags)

set(OOS_SECTOR2_OBJECT "${OOS_TARGET_OBJ_DIR}/sector2.o")

add_custom_command(
  OUTPUT "${OOS_SECTOR2_OBJECT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OOS_TARGET_OBJ_DIR}"
  COMMAND "${CMAKE_ASM_NASM_COMPILER}" -f elf32 "${CMAKE_CURRENT_SOURCE_DIR}/sector2.s" -o "${OOS_SECTOR2_OBJECT}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/sector2.s"
  VERBATIM
)

add_custom_target(oos_sector2_obj DEPENDS "${OOS_SECTOR2_OBJECT}")
set(OOS_SECTOR2_OBJECT "${OOS_SECTOR2_OBJECT}" PARENT_SCOPE)