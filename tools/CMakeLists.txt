set(FIX_SOURCES
    fix.clay
    fix/v0_0.clay
)

set(BINDGEN_SOURCES
    bindgen.clay
)

if (WIN32)
    set(EXE .exe)
    set(LIBCLANG libclang.dll)
    set(LLVM_PREFIX ${LLVM_DIR})
else (WIN32)
    set(EXE )
    set(LIBCLANG clang)
endif (WIN32)

if(UNIX)
    set(DEST bin)
else()
    set(DEST .)
endif()

add_custom_command(OUTPUT ${clay_BINARY_DIR}/tools/clay-fix${EXE}
    DEPENDS
        ${clay_BINARY_DIR}/compiler/src/clay
        ${FIX_SOURCES}
    COMMAND ${clay_BINARY_DIR}/compiler/src/clay
        -I${clay_SOURCE_DIR}/lib-clay
        -I${clay_SOURCE_DIR}/tools
        -O1
        -o ${clay_BINARY_DIR}/tools/clay-fix${EXE}
        ${clay_SOURCE_DIR}/tools/fix.clay)

add_custom_target(clay-fix ALL DEPENDS ${clay_BINARY_DIR}/tools/clay-fix${EXE})

if(BUILD_BINDGEN)
    add_custom_command(OUTPUT ${clay_BINARY_DIR}/tools/clay-bindgen${EXE}
        DEPENDS
            ${clay_BINARY_DIR}/compiler/src/clay
            ${BINDGEN_SOURCES}
        COMMAND ${clay_BINARY_DIR}/compiler/src/clay
            -I${clay_SOURCE_DIR}/lib-clay
            -I${clay_SOURCE_DIR}/tools
            -o ${clay_BINARY_DIR}/tools/clay-bindgen${EXE}
            -L${LLVM_PREFIX}/lib
            -L${LLVM_LIBDIR}
            -Wl,-rpath,${LLVM_PREFIX}/lib
            -l${LIBCLANG}
            ${clay_SOURCE_DIR}/tools/bindgen.clay)

    add_custom_target(clay-bindgen ALL DEPENDS ${clay_BINARY_DIR}/tools/clay-bindgen${EXE})
    install(PROGRAMS ${clay_BINARY_DIR}/tools/clay-bindgen${EXE} DESTINATION ${DEST})
endif(BUILD_BINDGEN)

install(PROGRAMS ${clay_BINARY_DIR}/tools/clay-fix${EXE} DESTINATION ${DEST})

