find_program(SIDLC_EXECUTABLE sidlc REQUIRED)

get_filename_component(SIDLC_INTERFACE_DIRECTORY ${SIDLC_EXECUTABLE} DIRECTORY)
get_filename_component(SIDLC_INTERFACE_DIRECTORY ${SIDLC_INTERFACE_DIRECTORY}/../lib/sidl/interfaces/ ABSOLUTE)

# =========================================================================
# sidl_generate_c
# Usage: sidl_generate_c(SRCS_VAR source_list_out_var_name HDRS_VAR header_list_out_var_name FILES file1.sidl file2.sidl ...)
# =========================================================================
function(sidl_generate_c)
    set(options)
    set(oneValueArgs HEADER_DIR SRCS_VAR HDRS_VAR)
    set(multiValueArgs FILES)
    cmake_parse_arguments(PARSE_ARGV 0 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    if(NOT arg_FILES)
        message(FATAL_ERROR "sidl_generate_c() called without any SIDL files.")
    endif()

    if (NOT arg_HEADER_DIR)
        set(arg_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    set(_generated_srcs)
    set(_generated_hdrs)

    file(MAKE_DIRECTORY "${arg_HEADER_DIR}")

    foreach(sidl_file ${arg_FILES})
        get_filename_component(abs_file ${sidl_file} ABSOLUTE)
        get_filename_component(basename ${sidl_file} NAME_WE)

        set(out_hdr "${arg_HEADER_DIR}/${basename}.h")
        set(out_src "${arg_HEADER_DIR}/${basename}.c")

        add_custom_command(
            OUTPUT ${out_hdr} ${out_src}
            COMMAND ${SIDLC_EXECUTABLE} 
                    --lang=c
                    --arch=${CMAKE_SYSTEM_PROCESSOR}
                    --header=${out_hdr} 
                    --user-src=${out_src}
                    ${abs_file}
            DEPENDS ${abs_file} ${SIDLC_EXECUTABLE}
            COMMENT "Compiling SIDL interface: ${basename}.sidl"
            VERBATIM
        )

        
        list(APPEND _generated_srcs ${out_src})
        list(APPEND _generated_hdrs ${out_hdr})
    endforeach()

    set(${arg_SRCS_VAR} ${_generated_srcs} PARENT_SCOPE)
    set(${arg_HDRS_VAR} ${_generated_hdrs} PARENT_SCOPE)
endfunction()
