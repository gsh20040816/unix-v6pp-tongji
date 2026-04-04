add_library(oos_tty_obj OBJECT
  TTy.cpp
  Keyboard.cpp
  CRT.cpp
)

target_link_libraries(oos_tty_obj PRIVATE oos_kernel_flags)