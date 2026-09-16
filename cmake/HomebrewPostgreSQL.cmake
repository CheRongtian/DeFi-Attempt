if(APPLE AND NOT PostgreSQL_ROOT)
    find_program(DLP_HOMEBREW_EXECUTABLE brew)
    if(DLP_HOMEBREW_EXECUTABLE)
        execute_process(
            COMMAND ${DLP_HOMEBREW_EXECUTABLE} --prefix libpq
            RESULT_VARIABLE DLP_HOMEBREW_LIBPQ_RESULT
            OUTPUT_VARIABLE DLP_HOMEBREW_LIBPQ_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(DLP_HOMEBREW_LIBPQ_RESULT EQUAL 0)
            set(PostgreSQL_ROOT "${DLP_HOMEBREW_LIBPQ_PREFIX}")
            list(PREPEND CMAKE_PREFIX_PATH "${DLP_HOMEBREW_LIBPQ_PREFIX}")
        endif()
    endif()
endif()
