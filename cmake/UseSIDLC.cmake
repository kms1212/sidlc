find_program(SIDLC_EXECUTABLE sidlc REQUIRED)

get_filename_component(SIDLC_INTERFACE_DIRECTORY ${SIDLC_EXECUTABLE} DIRECTORY)
get_filename_component(SIDLC_INTERFACE_DIRECTORY ${SIDLC_INTERFACE_DIRECTORY}/../lib/sidl/interfaces/ ABSOLUTE)

# =========================================================================
# sidl_generate_c
# Usage: sidl_generate_c(USER|MODULE SRCS_VAR source_list_out_var_name HDRS_VAR header_list_out_var_name FILES file1.sidl file2.sidl ...)
# Generates:
#   USER:   <name>.types.h, <name>.h, <name>.c
#   MODULE: <name>.types.h, <name>.module.h, <name>.module.c
# =========================================================================
function(sidl_generate_c)
    set(options USER MODULE)
    set(oneValueArgs HEADER_DIR SRCS_VAR HDRS_VAR)
    set(multiValueArgs FILES)
    cmake_parse_arguments(PARSE_ARGV 0 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    if(NOT arg_FILES)
        message(FATAL_ERROR "sidl_generate_c() called without any SIDL files.")
    endif()

    if(NOT arg_USER AND NOT arg_MODULE)
        message(FATAL_ERROR "sidl_generate_c() requires at least one of USER or MODULE.")
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

        set(out_types_hdr "${arg_HEADER_DIR}/${basename}.types.h")
        get_filename_component(out_types_hdr_name "${out_types_hdr}" NAME)
        set(outputs ${out_types_hdr})
        set(sidlc_args
            --lang=c
            --arch=${CMAKE_SYSTEM_PROCESSOR}
            --type-header=${out_types_hdr}
        )

        list(APPEND _generated_hdrs ${out_types_hdr})

        if(arg_USER)
            set(out_user_hdr "${arg_HEADER_DIR}/${basename}.h")
            set(out_user_src "${arg_HEADER_DIR}/${basename}.c")
            get_filename_component(out_user_hdr_name "${out_user_hdr}" NAME)

            list(APPEND outputs ${out_user_hdr} ${out_user_src})
            list(APPEND sidlc_args
                --user-header=${out_user_hdr}
                --user-header-type-path=${out_types_hdr_name}
                --user-src=${out_user_src}
                --user-src-header-path=${out_user_hdr_name}
            )

            list(APPEND _generated_hdrs ${out_user_hdr})
            list(APPEND _generated_srcs ${out_user_src})
        endif()

        if(arg_MODULE)
            set(out_module_hdr "${arg_HEADER_DIR}/${basename}.module.h")
            set(out_module_src "${arg_HEADER_DIR}/${basename}.module.c")
            get_filename_component(out_module_hdr_name "${out_module_hdr}" NAME)

            list(APPEND outputs ${out_module_hdr} ${out_module_src})
            list(APPEND sidlc_args
                --module-header=${out_module_hdr}
                --module-header-type-path=${out_types_hdr_name}
                --module-src=${out_module_src}
                --module-src-header-path=${out_module_hdr_name}
            )

            list(APPEND _generated_hdrs ${out_module_hdr})
            list(APPEND _generated_srcs ${out_module_src})
        endif()

        add_custom_command(
            OUTPUT ${outputs}
            COMMAND ${SIDLC_EXECUTABLE} ${sidlc_args} ${abs_file}
            DEPENDS ${abs_file} ${SIDLC_EXECUTABLE}
            COMMENT "Compiling SIDL interface: ${basename}.sidl"
            VERBATIM
        )
    endforeach()

    set(${arg_SRCS_VAR} ${_generated_srcs} PARENT_SCOPE)
    set(${arg_HDRS_VAR} ${_generated_hdrs} PARENT_SCOPE)
endfunction()
