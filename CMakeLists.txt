project(clay)
cmake_minimum_required(VERSION 2.6)

include(FindPythonInterp)

find_program(LLVM_CONFIG llvm-config ${LLVM_INSTDIR}/bin 
    DOC "path to llvm-config")

execute_process(
    COMMAND ${LLVM_CONFIG} --cxxflags
    OUTPUT_VARIABLE LLVM_CXXFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REPLACE "-fno-exc" "-fexc" LLVM_CXXFLAGS ${LLVM_CXXFLAGS})

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

set(CMAKE_OSX_ARCHITECTURES x86_64)

install(DIRECTORY lib-clay DESTINATION lib)

add_subdirectory(compiler)
add_subdirectory(misc)

if(PYTHONINTERP_FOUND)
    add_custom_target(test ${PYTHON_EXECUTABLE} "../test/runtests.py")
    add_dependencies(test clay)
else(PYTHONINTERP_FOUND)
    message("-- Python not found. You will not be able to run tests")
endif(PYTHONINTERP_FOUND)
