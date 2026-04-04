add_library(oos_kernel_obj OBJECT
  main.cpp
  Kernel.cpp
  Video.cpp
  Utility.cpp
)

target_link_libraries(oos_kernel_obj PRIVATE oos_kernel_flags)