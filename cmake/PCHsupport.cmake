# header to precompile    list of sources    source to precompile    additional arguments
macro(precompile_header hdr src pchsrc args)
    if(MSVC)
        set_source_files_properties(${pchsrc} PROPERTIES COMPILE_FLAGS /Yc${hdr})
        foreach(s ${${src}})
            set_source_files_properties(${s} PROPERTIES COMPILE_FLAGS /Yu${hdr})
        endforeach()
        list(INSERT ${src} 0 ${pchsrc})
    elseif(CMAKE_COMPILER_IS_GNUCXX OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        get_directory_property(_args COMPILE_FLAGS)
        list(APPEND _args ${args})
        get_directory_property(_incs INCLUDE_DIRECTORIES)
        foreach(i ${_incs})
            list(APPEND _args "-I${i}")
        endforeach()
        get_directory_property(_defs COMPILE_DEFINITIONS)
        foreach(d ${_defs})
            list(APPEND _args "-D${d}")
        endforeach()
        separate_arguments(_args)
        add_custom_target(gch ALL DEPENDS ${hdr}.gch)
        add_custom_command(OUTPUT ${hdr}.gch
            COMMAND ${CMAKE_CXX_COMPILER} -x c++-header ${CMAKE_CURRENT_SOURCE_DIR}/${hdr} ${_args} -o ${hdr}.gch
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${hdr}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            VERBATIM)
        set_source_files_properties(${${src}} PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${hdr}.gch
            COMPILE_FLAGS "-include ${CMAKE_CURRENT_BINARY_DIR}/${hdr}")
    endif()
endmacro()
