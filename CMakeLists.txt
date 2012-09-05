project(clay)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Subversion)
find_package(Mercurial)
find_package(Git)

set(Python_ADDITIONAL_VERSIONS "2.7")
include(FindPythonInterp)
include(CheckSymbolExists)

include(CPack)

if(UNIX)
    if(DEFINED LLVM_DIR)
        set(LLVM_CONFIG "${LLVM_DIR}/bin/llvm-config")
    else()
        find_program(LLVM_CONFIG llvm-config /usr/local/bin
            DOC "path to llvm-config")
    endif()
    message("-- Using llvm-config: ${LLVM_CONFIG}")

    execute_process(
        COMMAND ${LLVM_CONFIG} --version
        OUTPUT_VARIABLE LLVM_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE LLVM_CONFIG_VERSION_RESULT
    )

    if (NOT ${LLVM_CONFIG_VERSION_RESULT} EQUAL 0)
        message(FATAL_ERROR "${LLVM_CONFIG} failed")
    endif()

    if(NOT ${LLVM_VERSION} MATCHES "^3.1(svn)?$")
        message(FATAL_ERROR "Clay requires LLVM 3.1.")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG} --cxxflags
        OUTPUT_VARIABLE LLVM_CXXFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(LLVM_CXXFLAGS "${LLVM_CXXFLAGS}")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # llvm-config --cxxflags contains -O3
        set(LLVM_CXXFLAGS "${LLVM_CXXFLAGS} -O0")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG} --ldflags
        OUTPUT_VARIABLE LLVM_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${LLVM_CONFIG} --libs all
        OUTPUT_VARIABLE LLVM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${LLVM_CONFIG} --libdir
        OUTPUT_VARIABLE LLVM_LIBDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${LLVM_CONFIG} --prefix
        OUTPUT_VARIABLE LLVM_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message("-- Using LLVM: ${LLVM_DIR}")
    set(CMAKE_REQUIRED_INCLUDES ${LLVM_DIR}/include)
    set(CMAKE_REQUIRED_LIBRARIES clang)
    set(CMAKE_REQUIRED_FLAGS "-L${LLVM_LIBDIR}")
elseif(WIN32)
    set(LLVM_DIR "$ENV{ProgramFiles}/LLVM" CACHE PATH "llvm install path")

    if(IS_DIRECTORY ${LLVM_DIR})
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}/share/llvm/cmake")
        include(LLVMConfig)

        include_directories( ${LLVM_DIR}/include )
        link_directories( ${LLVM_DIR}/lib )
        add_definitions( ${LLVM_DEFINITIONS} )
        llvm_map_components_to_libraries(LLVM_LIBS ${LLVM_TARGETS_TO_BUILD} jit interpreter native asmparser bitreader bitwriter ipo)
        find_file(CLANG clang.exe HINTS ${LLVM_DIR}/bin)
    else()
        message(FATAL_ERROR "Could not find LLVM. Try setting LLVM_DIR")
    endif()
    set(CMAKE_REQUIRED_INCLUDES ${LLVM_DIR}/include)
    set(CMAKE_REQUIRED_LIBRARIES ${LLVM_DIR}/lib/libclang.lib)
endif()

set(BUILD_BINDGEN True CACHE BOOL
    "Build the clay-bindgen tool for generating Clay bindings from C header files.")

install(DIRECTORY doc/ DESTINATION share/doc/clay)
install(DIRECTORY examples/ DESTINATION share/doc/clay/examples)
install(DIRECTORY lib-clay DESTINATION lib)

add_subdirectory(compiler)
add_subdirectory(misc)
add_subdirectory(tools)

if(PYTHONINTERP_FOUND)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} --version
        ERROR_VARIABLE PYTHON_VERSION
        ERROR_STRIP_TRAILING_WHITESPACE)

    if(NOT ${PYTHON_VERSION} MATCHES "Python 2")
        unset(PYTHON_EXECUTABLE)
        find_program(PYTHON2_EXE python2)
        if(PYTHON2_EXE)
            set(PYTHON_EXECUTABLE ${PYTHON2_EXE})
        endif()
    endif()

    if(DEFINED PYTHON_EXECUTABLE)
        add_custom_target(test ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/test/runtests.py" --buildflags="-Dtest.minimal")
        add_dependencies(test clay)
    else()
        message("-- Python 2.x not found. You will not be able to run tests")
        message("-- Try setting PYTHON_EXECUTABLE to the Python 2.x interpreter")
    endif()
endif()
