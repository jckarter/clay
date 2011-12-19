project(clay)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Subversion)
find_package(Mercurial)
find_package(Git)

set(Python_ADDITIONAL_VERSIONS "2.7")
include(FindPythonInterp)

if(UNIX)
    find_program(LLVM_CONFIG llvm-config ${LLVM_INSTDIR}/bin 
        DOC "path to llvm-config")

    execute_process(
        COMMAND ${LLVM_CONFIG} --version
        OUTPUT_VARIABLE LLVM_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT ${LLVM_VERSION} STREQUAL "3.0")
        message(FATAL_ERROR "Clay requires LLVM 3.0.")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG} --cxxflags
        OUTPUT_VARIABLE LLVM_CXXFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(LLVM_CXXFLAGS "${LLVM_CXXFLAGS} -fno-rtti")

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
        COMMAND ${LLVM_CONFIG} --src-root
        OUTPUT_VARIABLE LLVM_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${LLVM_CONFIG} --prefix
        OUTPUT_VARIABLE LLVM_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message("-- Testing whether bindgen can be built")
    execute_process(
        COMMAND ${clay_SOURCE_DIR}/tools/can-build-bindgen.sh
            ${LLVM_PREFIX}
            ${LLVM_LDFLAGS}
            ${LLVM_LIBS}
            ${LLVM_CXXFLAGS}
        RESULT_VARIABLE BUILD_BINDGEN_RESULT
    )
    if(${BUILD_BINDGEN_RESULT} EQUAL 0)
        set(BUILD_BINDGEN True)
        message("   Yes")
    else()
        set(BUILD_BINDGEN False)
        message("   libclang is either missing or needs to be patched to support")
        message("   bindgen. bindgen will not be built. Try applying the patch in")
        message("   tools/libclang.diff to the Clang source.")
    endif()
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

    if(CLANG)
        find_path(GCC_DIR gcc.exe)
        if(EXISTS ${GCC_DIR}/../include)
            file(WRITE ${PROJECT_BINARY_DIR}/test-libclang.c
                "#include <clang-c/Index.h>\nint main() {CXCursor c;CXType t = clang_getTypedefDeclUnderlyingType(c);}"
            )
            execute_process(
                COMMAND ${CLANG} ${PROJECT_BINARY_DIR}/test-libclang.c -I${GCC_DIR}/../include ${LLVM_DIR}/lib/liblibclang.dll.a
                OUTPUT_QUIET
                RESULT_VARIABLE BUILD_BINDGEN_RESULT
            )
        else()
            message("mingw headers not found, cannot test for patched libclang")
        endif()
        if(BUILD_BINDGEN_RESULT EQUAL 0)
            set(BUILD_BINDGEN True)
        else()
            message("no compatibly patched libclang found, bindgen will not be built")
        endif()
    else()
        message("-- Clang not found. clay-bindgen will not be built")
    endif()
endif(UNIX)

if(UNIX)
    install(DIRECTORY lib-clay DESTINATION lib)
else(UNIX)
    install(DIRECTORY lib-clay DESTINATION .)
endif(UNIX)

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
        add_custom_target(test ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/test/runtests.py")
        add_dependencies(test clay)
    else()
        message("-- Python 2.x not found. You will not be able to run tests")
        message("-- Try setting PYTHON_EXECUTABLE to the Python 2.x interpreter")
    endif()
endif()
