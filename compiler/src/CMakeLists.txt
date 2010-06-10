set(SOURCES
    analyzer.cpp
    clone.cpp
    codegen.cpp
    constructors.cpp
    desugar.cpp
    env.cpp
    error.cpp
    evaluator.cpp
    invoketables.cpp
    lambdas.cpp
    lexer.cpp
    literals.cpp
    llvm.cpp
    loader.cpp
    main.cpp
    matchinvoke.cpp
    parser.cpp
    patterns.cpp
    printer.cpp
    types.cpp
)

add_executable(clay ${SOURCES})

if (UNIX)
    set_target_properties(clay PROPERTIES LINK_FLAGS ${LLVM_LDFLAGS})
    set_target_properties(clay PROPERTIES COMPILE_FLAGS ${LLVM_CXXFLAGS})
else(MSVC)
    set(IGNORE_WARNINGS 4146 4503 4996 4800 4244 4624 4355
        4715 4180 4345 4224 4351)
    set_target_properties(clay PROPERTIES COMPILE_FLAGS 
        "/D_CRT_SECURE_NO_DEPRECATE /D_CRT_SECURE_NO_WARNINGS 
         /D_SCL_SECURE_NO_WARNINGS /DCRT_NONSTDC_NO_WARNINGS 
         /D_SCL_SECURE_NO_DEPRECATE /wd${IGNORE_WARNINGS}")
endif (UNIX)

target_link_libraries(clay ${LLVM_LIBS})

install(TARGETS clay RUNTIME DESTINATION bin)
