if(NOT SIDLC_EXECUTABLE)
    find_program(SIDLC_EXECUTABLE sidlc)
endif()

if(NOT SIDLC_EXECUTABLE)
    message(FATAL_ERROR "sidlc executable not found. Set SIDLC_EXECUTABLE or install sidlc in PATH.")
endif()

# =========================================================================
# sidl_generate_c
# Usage: sidl_generate_c(CLIENT|SERVER|SERVER_CLIENT SRCS_VAR source_list_out_var_name HDRS_VAR header_list_out_var_name FILES file1.sidl file2.sidl ...)
# Generates:
#   CLIENT:        <name>.types.h, <name>.h, <name>.c
#   SERVER:        <name>.types.h, <name>.server.h, <name>.server.c
#   SERVER_CLIENT: <name>.types.h, <name>.server-client.h, <name>.server-client.c
# =========================================================================
function(sidl_generate_c)
    set(options CLIENT SERVER SERVER_CLIENT)
    set(oneValueArgs HEADER_DIR SRCS_VAR HDRS_VAR)
    set(multiValueArgs FILES)
    cmake_parse_arguments(PARSE_ARGV 0 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    if(NOT arg_FILES)
        message(FATAL_ERROR "sidl_generate_c() called without any SIDL files.")
    endif()

    if(NOT arg_CLIENT AND NOT arg_SERVER AND NOT arg_SERVER_CLIENT)
        message(FATAL_ERROR "sidl_generate_c() requires at least one of CLIENT, SERVER, or SERVER_CLIENT.")
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

        set(out_sif "${arg_HEADER_DIR}/${basename}.sif")
        set(out_types_hdr "${arg_HEADER_DIR}/${basename}.types.h")
        set(outputs ${out_types_hdr})
        set(generate_commands)

        add_custom_command(
            OUTPUT ${out_sif}
            COMMAND ${SIDLC_EXECUTABLE} compile -o ${out_sif} ${abs_file}
            DEPENDS ${abs_file} ${SIDLC_EXECUTABLE}
            COMMENT "Compiling SIDL interface artifact: ${basename}.sif"
            VERBATIM
        )
        set_source_files_properties(${out_sif} PROPERTIES GENERATED TRUE)

        list(APPEND _generated_hdrs ${out_types_hdr})

        if(arg_CLIENT)
            set(out_client_hdr "${arg_HEADER_DIR}/${basename}.h")
            set(out_client_src "${arg_HEADER_DIR}/${basename}.client.c")

            list(APPEND outputs ${out_client_hdr} ${out_client_src})
            list(APPEND generate_commands
                COMMAND ${SIDLC_EXECUTABLE} generate
                    -a ${CMAKE_SYSTEM_PROCESSOR}
                    -l c
                    -m client
                    -h ${arg_HEADER_DIR}
                    -s ${out_client_src}
                    ${out_sif}
            )

            list(APPEND _generated_hdrs ${out_client_hdr})
            list(APPEND _generated_srcs ${out_client_src})
        endif()

        if(arg_SERVER)
            set(out_server_hdr "${arg_HEADER_DIR}/${basename}.server.h")
            set(out_server_src "${arg_HEADER_DIR}/${basename}.server.c")

            list(APPEND outputs ${out_server_hdr} ${out_server_src})
            list(APPEND generate_commands
                COMMAND ${SIDLC_EXECUTABLE} generate
                    -a ${CMAKE_SYSTEM_PROCESSOR}
                    -l c
                    -m server
                    -h ${arg_HEADER_DIR}
                    -s ${out_server_src}
                    ${out_sif}
            )

            list(APPEND _generated_hdrs ${out_server_hdr})
            list(APPEND _generated_srcs ${out_server_src})
        endif()

        if(arg_SERVER_CLIENT)
            set(out_server_client_hdr "${arg_HEADER_DIR}/${basename}.server-client.h")
            set(out_server_client_src "${arg_HEADER_DIR}/${basename}.server-client.c")

            list(APPEND outputs ${out_server_client_hdr} ${out_server_client_src})
            list(APPEND generate_commands
                COMMAND ${SIDLC_EXECUTABLE} generate
                    -a ${CMAKE_SYSTEM_PROCESSOR}
                    -l c
                    -m server-client
                    -h ${arg_HEADER_DIR}
                    -s ${out_server_client_src}
                    ${out_sif}
            )

            list(APPEND _generated_hdrs ${out_server_client_hdr})
            list(APPEND _generated_srcs ${out_server_client_src})
        endif()

        add_custom_command(
            OUTPUT ${outputs}
            ${generate_commands}
            DEPENDS ${out_sif} ${SIDLC_EXECUTABLE}
            COMMENT "Generating C bindings from SIDL interface artifact: ${basename}.sif"
            VERBATIM
        )
        set_source_files_properties(${outputs} PROPERTIES GENERATED TRUE)
    endforeach()

    set(${arg_SRCS_VAR} ${_generated_srcs} PARENT_SCOPE)
    set(${arg_HDRS_VAR} ${_generated_hdrs} PARENT_SCOPE)
endfunction()
