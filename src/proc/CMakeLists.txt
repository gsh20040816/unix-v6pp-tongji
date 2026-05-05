add_library(oos_proc_obj OBJECT
  Process.cpp
  ProcessManager.cpp
  Semaphore.cpp
  MemoryDescriptor.cpp
  User.cpp
  Text.cpp
)

target_link_libraries(oos_proc_obj PRIVATE oos_kernel_flags)
