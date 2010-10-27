add_subdirectory(bindgen)
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
     add_subdirectory(Xcode)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
