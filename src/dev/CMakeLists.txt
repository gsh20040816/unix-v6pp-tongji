add_library(oos_dev_obj OBJECT
  BufferManager.cpp
  BlockDevice.cpp
  DeviceManager.cpp
  ATADriver.cpp
  DMA.cpp
  CharDevice.cpp
)

target_link_libraries(oos_dev_obj PRIVATE oos_kernel_flags)