add_library(oos_fs_obj OBJECT
  FileSystem.cpp
  OpenFileManager.cpp
  INode.cpp
  File.cpp
  FileManager.cpp
)

target_link_libraries(oos_fs_obj PRIVATE oos_kernel_flags)