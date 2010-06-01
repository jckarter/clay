set(SOURCES 
    analyzer.cpp            error.cpp               loader.cpp
    analyzer2.cpp           evaluator.cpp           main.cpp
    clone.cpp               invoketables.cpp        matchinvoke.cpp
    codegen.cpp             lambdas.cpp             parser.cpp
    constructors.cpp        lexer.cpp               patterns.cpp
    desugar.cpp             literals.cpp            printer.cpp
    env.cpp                 llvm.cpp                types.cpp
)
set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS 
    ${LLVM_CXXFLAGS})
add_executable(clay ${SOURCES})
set_target_properties(clay PROPERTIES LINK_FLAGS ${LLVM_LDFLAGS})
target_link_libraries(clay ${LLVM_LIBS})
