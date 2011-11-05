set(CLAYFIX_SOURCES
    clayfix.clay
    clayfix/v0_0.clay
)

if (WIN32)
    set(EXE .exe)
else (WIN32)
    set(EXE )
endif (WIN32)

add_custom_command(OUTPUT ${clay_BINARY_DIR}/tools/clayfix${EXE}
    DEPENDS
        ${clay_BINARY_DIR}/compiler/src/clay
        ${CLAYFIX_SOURCES}
    COMMAND ${clay_BINARY_DIR}/compiler/src/clay
        -I${clay_SOURCE_DIR}/lib-clay
        -I${clay_SOURCE_DIR}/tools
        -o ${clay_BINARY_DIR}/tools/clayfix${EXE}
        ${clay_SOURCE_DIR}/tools/clayfix.clay)

add_custom_target(clayfix ALL DEPENDS ${clay_BINARY_DIR}/tools/clayfix${EXE})

if (UNIX)
    install(PROGRAMS ${clay_BINARY_DIR}/tools/clayfix${EXE} DESTINATION bin)
else (UNIX)
    install(PROGRAMS ${clay_BINARY_DIR}/tools/clayfix${EXE} DESTINATION .)
endif (UNIX)

