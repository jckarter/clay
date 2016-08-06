set(BINDGEN_SOURCES
    bindgen.clay
)

if (WIN32)
    if(MSVC)
        set(LIBCLANG_LDFLAGS -Wl,${LLVM_DIR}/lib/libclang.lib)
    else()
        set(LIBCLANG_LDFLAGS -L${LLVM_DIR}/lib -Wl,-rpath,${LLVM_DIR}/lib -llibclang.dll)
    endif()
else (WIN32)
    set(LIBCLANG_LDFLAGS -L${LLVM_LIBDIR} -Wl,-rpath,${LLVM_LIBDIR} -lclang)
endif (WIN32)


if(BUILD_BINDGEN)
    add_custom_command(OUTPUT ${clay_BINARY_DIR}/tools/clay-bindgen${CMAKE_EXECUTABLE_SUFFIX}
        DEPENDS
            clay
            ${BINDGEN_SOURCES}
        COMMAND clay
            -Dlibclang_include=${LLVM_DIR}/lib/clang/${LLVM_VERSION}/include
            -I${clay_SOURCE_DIR}/lib-clay
            -I${clay_SOURCE_DIR}/tools
            -o ${clay_BINARY_DIR}/tools/clay-bindgen${CMAKE_EXECUTABLE_SUFFIX}
            ${LIBCLANG_LDFLAGS}
            ${clay_SOURCE_DIR}/tools/bindgen.clay)
            

    add_custom_target(clay-bindgen ALL
        DEPENDS ${clay_BINARY_DIR}/tools/clay-bindgen${CMAKE_EXECUTABLE_SUFFIX})
    install(PROGRAMS ${clay_BINARY_DIR}/tools/clay-bindgen${CMAKE_EXECUTABLE_SUFFIX}
        DESTINATION bin)
endif(BUILD_BINDGEN)
