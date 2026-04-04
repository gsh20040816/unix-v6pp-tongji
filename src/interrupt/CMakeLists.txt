add_library(oos_interrupt_obj OBJECT
  Exception.cpp
  SystemCall.cpp
  DiskInterrupt.cpp
  KeyboardInterrupt.cpp
  TimeInterrupt.cpp
)

target_link_libraries(oos_interrupt_obj PRIVATE oos_kernel_flags)