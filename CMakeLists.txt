project(clay)
cmake_minimum_required(VERSION 2.6)

find_program(LLVM_CONFIG llvm-config ${LLVM_INSTDIR}/bin 
    DOC "path to llvm-config")
string(REPLACE "/bin/llvm-config" "" LLVM_DIR ${LLVM_CONFIG})

execute_process(
    COMMAND ${LLVM_CONFIG} --cxxflags
    OUTPUT_VARIABLE LLVM_CXXFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
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

add_subdirectory(compiler)
