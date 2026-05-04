add_library(oos_mm_obj OBJECT
  Allocator.cpp
  PageManager.cpp
  SwapManager.cpp
  KernelAllocator.cpp
  New.cpp
)

set_source_files_properties(New.cpp PROPERTIES COMPILE_OPTIONS "-fcheck-new")
target_link_libraries(oos_mm_obj PRIVATE oos_kernel_flags)
