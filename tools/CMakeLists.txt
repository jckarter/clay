set(FIX_SOURCES
    fix.clay
    fix/v0_0.clay
)

set(BINDGEN_SOURCES
    bindgen.clay
)

if (WIN32)
    set(EXE .exe)
    set(LIBCLANG_LDFLAGS -Wl,${LLVM_DIR}/lib/libclang.lib)
else (WIN32)
    set(EXE )
    set(LIBCLANG_LDFLAGS -L${LLVM_LIBDIR} -Wl,-rpath,${LLVM_LIBDIR} -lclang)
endif (WIN32)

if(UNIX)
    set(DEST bin)
else()
    set(DEST .)
endif()

if(BUILD_FIX)
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
    install(PROGRAMS ${clay_BINARY_DIR}/tools/clay-fix${EXE} DESTINATION ${DEST})
endif(BUILD_FIX)

if(BUILD_BINDGEN)
    add_custom_command(OUTPUT ${clay_BINARY_DIR}/tools/clay-bindgen${EXE}
        DEPENDS
            ${clay_BINARY_DIR}/compiler/src/clay
            ${BINDGEN_SOURCES}
        COMMAND ${clay_BINARY_DIR}/compiler/src/clay
            -I${clay_SOURCE_DIR}/lib-clay
            -I${clay_SOURCE_DIR}/tools
            -o ${clay_BINARY_DIR}/tools/clay-bindgen${EXE}
            ${LIBCLANG_LDFLAGS}
            ${clay_SOURCE_DIR}/tools/bindgen.clay)

    add_custom_target(clay-bindgen ALL DEPENDS ${clay_BINARY_DIR}/tools/clay-bindgen${EXE})
    install(PROGRAMS ${clay_BINARY_DIR}/tools/clay-bindgen${EXE} DESTINATION ${DEST})
endif(BUILD_BINDGEN)


