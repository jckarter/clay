SET(Mercurial_FOUND FALSE)

FIND_PROGRAM(Mercurial_EXECUTABLE hg DOC "Mercurial command line client")
MARK_AS_ADVANCED(Mercurial_EXECUTABLE)

IF(Mercurial_EXECUTABLE)
    SET(Mercurial_FOUND TRUE)

    EXECUTE_PROCESS(COMMAND ${Mercurial_EXECUTABLE} version
        OUTPUT_VARIABLE Mercurial_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    STRING(REGEX REPLACE ".*version ([.0-9]+).*"
        "\\1" Mercurial_VERSION "${Mercurial_VERSION}")

    MACRO(Mercurial_WC_INFO dir prefix)
        EXECUTE_PROCESS(COMMAND ${Mercurial_EXECUTABLE} log -l1 ${dir}
            OUTPUT_VARIABLE ${prefix}_WC_INFO
            ERROR_VARIABLE Mercurial_info_error
            RESULT_VARIABLE Mercurial_info_result
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        IF(Mercurial_info_result EQUAL 0)
            STRING(REGEX REPLACE "^(.*\n)?changeset: *([^\n]+).*"
                "\\2" ${prefix}_WC_CHANGESET "${${prefix}_WC_INFO}")
            STRING(REGEX REPLACE "^(.*\n)?tag: *([^\n]+).*"
                "\\2" ${prefix}_WC_TAG "${${prefix}_WC_INFO}")
            STRING(REGEX REPLACE "^(.*\n)?user: *([^\n]+).*"
                "\\2" ${prefix}_WC_USER "${${prefix}_WC_INFO}")
            STRING(REGEX REPLACE "^(.*\n)?date: *([^\n]+).*"
                "\\2" ${prefix}_WC_DATE "${${prefix}_WC_INFO}")
        ELSE()
            MESSAGE(SEND_ERROR "\"${Mercurial_EXECUTABLE} log -l1 ${dir}\" failed with output:\n${Mercurial_info_error}")
        ENDIF()
    ENDMACRO()
ENDIF()
