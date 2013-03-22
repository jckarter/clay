include(PCHsupport)

set(COMPILER_SOURCES
    analyzer.cpp
    analyzer_op.cpp
    clone.cpp
    codegen.cpp
    codegen_op.cpp
    constructors.cpp
    desugar.cpp
    env.cpp
    error.cpp
    evaluator.cpp
    evaluator_op.cpp
    externals.cpp
    hirestimer.cpp
    interactive.cpp
    invoketables.cpp
    lambdas.cpp
    lexer.cpp
    literals.cpp
    loader.cpp
    matchinvoke.cpp
    objects.cpp
    parachute.cpp
    parser.cpp
    patterns.cpp
    printer.cpp
    profiler.cpp
    types.cpp
)

set(CLAY_SOURCES
    clay.cpp
)

set(CLAYDOC_SOURCES
    claydoc.cpp
    html.cpp
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

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_WC_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    set_property(SOURCE main.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS "GIT_ID=\"${GIT_WC_ID}\"")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CLAY_CXXFLAGS "${LLVM_CXXFLAGS} -UNDEBUG")
else()
    set(CLAY_CXXFLAGS "${LLVM_CXXFLAGS} -DNDEBUG")
endif()
if (NOT MSVC)
    set(CLAY_CXXFLAGS "${CLAY_CXXFLAGS} -fexceptions")
endif()

set(ENABLE_PCH False CACHE BOOL
    "Use precompiled headers when building the compiler. (experimental)")

if(ENABLE_PCH)
precompile_header(clay.hpp SOURCES pch.cpp "${CLAY_CXXFLAGS}")
endif(ENABLE_PCH)

add_library(compiler STATIC ${COMPILER_SOURCES})
add_executable(clay ${CLAY_SOURCES})
add_executable(claydoc ${CLAYDOC_SOURCES})
set_target_properties(compiler PROPERTIES COMPILE_FLAGS "${CLAY_CXXFLAGS}")
set_target_properties(clay PROPERTIES COMPILE_FLAGS "${CLAY_CXXFLAGS}")
set_target_properties(claydoc PROPERTIES COMPILE_FLAGS "${CLAY_CXXFLAGS}")

if (UNIX)
    set_target_properties(compiler PROPERTIES LINK_FLAGS "${LLVM_LDFLAGS}")
    set_target_properties(clay PROPERTIES LINK_FLAGS "${LLVM_LDFLAGS}")
    set_target_properties(claydoc PROPERTIES LINK_FLAGS "${LLVM_LDFLAGS}")
endif(UNIX)

install(TARGETS clay RUNTIME DESTINATION bin)
install(TARGETS claydoc RUNTIME DESTINATION bin)

target_link_libraries(clay compiler ${LLVM_LIBS})
target_link_libraries(claydoc compiler ${LLVM_LIBS})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(clay "rt")
    target_link_libraries(clay "dl")
    target_link_libraries(clay "pthread")
    target_link_libraries(claydoc "rt")
    target_link_libraries(claydoc "dl")
    target_link_libraries(claydoc "pthread")
endif()
