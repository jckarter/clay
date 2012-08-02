include(PCHsupport)

set(SOURCES
    ../../compiler/src/analyzer.cpp
    ../../compiler/src/clone.cpp
    ../../compiler/src/codegen.cpp
    ../../compiler/src/constructors.cpp
    ../../compiler/src/desugar.cpp
    ../../compiler/src/env.cpp
    ../../compiler/src/error.cpp
    ../../compiler/src/evaluator.cpp
    ../../compiler/src/externals.cpp
    ../../compiler/src/hirestimer.cpp
    ../../compiler/src/invoketables.cpp
    ../../compiler/src/lambdas.cpp
    ../../compiler/src/lexer.cpp
    ../../compiler/src/literals.cpp
    ../../compiler/src/loader.cpp
    ../../compiler/src/matchinvoke.cpp
    ../../compiler/src/objects.cpp
    ../../compiler/src/parachute.cpp
    ../../compiler/src/parser.cpp
    ../../compiler/src/patterns.cpp
    ../../compiler/src/printer.cpp
    ../../compiler/src/profiler.cpp
    ../../compiler/src/types.cpp
    main.cpp
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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CLAY_CXXFLAGS "${LLVM_CXXFLAGS} -UNDEBUG")
else()
    set(CLAY_CXXFLAGS "${LLVM_CXXFLAGS} -DNDEBUG")
endif()

set(ENABLE_PCH False CACHE BOOL
    "Use precompiled headers when building the compiler. (experimental)")

if(ENABLE_PCH)
precompile_header(../../compiler/src/clay.hpp SOURCES ../../compiler/src/pch.cpp "${CLAY_CXXFLAGS}")
endif(ENABLE_PCH)

add_executable(claydoc ${SOURCES})
set_target_properties(claydoc PROPERTIES COMPILE_FLAGS "${CLAY_CXXFLAGS}")

if (UNIX)
    set_target_properties(claydoc PROPERTIES LINK_FLAGS "${LLVM_LDFLAGS}")
endif(UNIX)

install(TARGETS claydoc RUNTIME DESTINATION bin)

target_link_libraries(claydoc ${LLVM_LIBS})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(claydoc "rt")
    target_link_libraries(claydoc "dl")
    target_link_libraries(claydoc "pthread")
endif()


