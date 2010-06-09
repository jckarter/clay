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

set_target_properties(clay PROPERTIES COMPILE_FLAGS "${LLVM_CXXFLAGS}")

if (UNIX)
    set_target_properties(clay PROPERTIES LINK_FLAGS ${LLVM_LDFLAGS})
endif(UNIX)

target_link_libraries(clay ${LLVM_LIBS})

install(TARGETS clay RUNTIME DESTINATION bin)
