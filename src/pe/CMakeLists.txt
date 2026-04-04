add_library(oos_pe_obj OBJECT
  ELFParser.cpp
)

target_link_libraries(oos_pe_obj PRIVATE oos_kernel_flags)