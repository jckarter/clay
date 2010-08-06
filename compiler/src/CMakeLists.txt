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
    hirestimer.cpp
)

# version info is only updated when cmake is run
if(Subversion_FOUND AND EXISTS "${LLVM_DIR}/.svn")
    Subversion_WC_INFO(${LLVM_DIR} SVN)
    set_property(SOURCE main.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS "SVN_REVISION=\"${SVN_WC_REVISION}\"")
endif()

if(Mercurial_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.hg")
    Mercurial_WC_ID(${PROJECT_SOURCE_DIR} HG)
    set_property(SOURCE main.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS "HG_ID=\"${HG_WC_ID}\"")
endif()

add_executable(clay ${SOURCES})

set_target_properties(clay PROPERTIES COMPILE_FLAGS "${LLVM_CXXFLAGS}")

if (UNIX)
    set_target_properties(clay PROPERTIES LINK_FLAGS ${LLVM_LDFLAGS})
    install(TARGETS clay RUNTIME DESTINATION bin)
else(UNIX)
    install(TARGETS clay RUNTIME DESTINATION .)
endif(UNIX)

target_link_libraries(clay ${LLVM_LIBS})
