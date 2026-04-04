add_library(oos_machine_obj OBJECT
  Chip8253.cpp
  Chip8259A.cpp
  IDT.cpp
  GDT.cpp
  Machine.cpp
  TaskStateSegment.cpp
  CMOSTime.cpp
)

target_link_libraries(oos_machine_obj PRIVATE oos_kernel_flags)