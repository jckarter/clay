set(SOURCES
    BindingsConverter.cpp
    InitHeaderSearch.cpp
    clay-bindgen.cpp
)

add_executable(clay-bindgen ${SOURCES})
if (UNIX)
    set_target_properties(clay-bindgen PROPERTIES LINK_FLAGS ${LLVM_LDFLAGS})
    set_target_properties(clay-bindgen PROPERTIES COMPILE_FLAGS 
        ${LLVM_CXXFLAGS})
endif (UNIX)
target_link_libraries(clay-bindgen 
    "clangFrontend" "clangSema" "clangChecker" "clangAnalysis" 
    "clangAST" "clangParse" "clangLex" "clangBasic" ${LLVM_LIBS})
