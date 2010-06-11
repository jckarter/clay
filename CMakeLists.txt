project(clay)
cmake_minimum_required(VERSION 2.6)

include(FindPythonInterp)

if(UNIX)
    find_program(LLVM_CONFIG llvm-config ${LLVM_INSTDIR}/bin 
        DOC "path to llvm-config")
    find_program(CLANG clang ${LLVM_INSTDIR}/bin 
        DOC "path to clang")

    if(CLANG) 
        set(BUILD_BINDGEN True)
    else()
        message("-- Clang not found. clay-bindgen will not be built")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG} --cxxflags
        OUTPUT_VARIABLE LLVM_CXXFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE "-fno-exc" "-fexc" LLVM_CXXFLAGS ${LLVM_CXXFLAGS})
    set(LLVM_CXXFLAGS "${LLVM_CXXFLAGS} -Wall")

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
elseif(MSVC)
    set(LLVM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../llvm)
    set(LLVM_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../llvm-build)
    set(CLANG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../llvm/tools/clang)

    if(IS_DIRECTORY ${LLVM_BUILD_DIR})
        link_directories(${LLVM_BUILD_DIR}/lib/Release)
    else()
        message(FATAL_ERROR "Could not find LLVM build directory")
    endif()

    if(IS_DIRECTORY ${LLVM_DIR})
        include_directories(${LLVM_BUILD_DIR}/include ${LLVM_DIR}/include)
    else()
        message(FATAL_ERROR "Could not find LLVM souce directory")
    endif()

    if(IS_DIRECTORY ${CLANG_DIR})
        set(BUILD_BINDGEN True)
        include_directories(${CLANG_DIR}/include)
        include_directories(${LLVM_BUILD_DIR}/tools/clang/include)
    else()
        message("-- Clang not found. clay-bindgen will not be built")
    endif()

    set(IGNORE_WARNINGS 4146 4503 4996 4800 4244 4624 4355
        4715 4180 4345 4224 4351)
    set(LLVM_CXXFLAGS 
        "/D_CRT_SECURE_NO_DEPRECATE /D_CRT_SECURE_NO_WARNINGS 
         /D_SCL_SECURE_NO_WARNINGS /DCRT_NONSTDC_NO_WARNINGS 
         /D_SCL_SECURE_NO_DEPRECATE /wd${IGNORE_WARNINGS}")

    set(LLVM_LIBS 
        "LLVMJIT" "LLVMInterpreter" "LLVMExecutionEngine" "LLVMX86AsmPrinter" 
        "LLVMX86AsmParser" "LLVMX86Disassembler" "LLVMBitReader" 
        "LLVMBitWriter" "LLVMipo" "LLVMX86CodeGen" "LLVMMCParser" 
        "LLVMSelectionDAG" "LLVMX86Info" "LLVMAsmPrinter" "LLVMCodeGen" 
        "LLVMScalarOpts" "LLVMInstCombine" "LLVMTransformUtils" "LLVMipa" 
        "LLVMAnalysis" "LLVMTarget" "LLVMCore" "LLVMMC" "LLVMSupport"
        "LLVMSystem")
endif(UNIX)

set(CMAKE_OSX_ARCHITECTURES x86_64)

install(DIRECTORY lib-clay DESTINATION lib)

add_subdirectory(compiler)
add_subdirectory(misc)

if(PYTHONINTERP_FOUND)
    add_custom_target(test ${PYTHON_EXECUTABLE} "../test/runtests.py")
    add_dependencies(test clay)
else()
    message("-- Python not found. You will not be able to run tests")
endif()
