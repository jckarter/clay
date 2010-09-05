set(SOURCES
    BindingsConverter.cpp
    InitHeaderSearch.cpp
    clay-bindgen.cpp
)

add_executable(clay-bindgen ${SOURCES})

set_target_properties(clay-bindgen PROPERTIES COMPILE_FLAGS "${LLVM_CXXFLAGS}")

if (UNIX)
    set_target_properties(clay-bindgen PROPERTIES LINK_FLAGS ${LLVM_LDFLAGS})
    install(TARGETS clay-bindgen RUNTIME DESTINATION bin)
else(UNIX)
    install(TARGETS clay-bindgen RUNTIME DESTINATION .)
endif (UNIX)

target_link_libraries(clay-bindgen 
    "clangFrontend" "clangParse" "clangSema" "clangChecker"
    "clangAnalysis" "clangIndex" "clangAST" "clangLex"
    "clangBasic" ${LLVM_LIBS})
