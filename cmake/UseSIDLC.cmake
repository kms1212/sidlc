if(NOT SIDLC_EXECUTABLE)
    find_program(SIDLC_EXECUTABLE sidlc)
endif()

if(NOT SIDLC_EXECUTABLE)
    message(FATAL_ERROR "sidlc executable not found. Set SIDLC_EXECUTABLE or install sidlc in PATH.")
endif()

# =========================================================================
# sidlc_compile
# Usage:
#   sidlc_compile(
#       OUTPUT_DIR <dir>
#       SIFS_VAR <sif_list_out_var_name>
#       FILES <file.sidl>...)
#
# Compiles human-authored .sidl files into binary .sif interface artifacts.
# =========================================================================
function(sidlc_compile)
    set(options)
    set(oneValueArgs OUTPUT_DIR SIFS_VAR)
    set(multiValueArgs FILES)
    cmake_parse_arguments(PARSE_ARGV 0 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    if(NOT arg_FILES)
        message(FATAL_ERROR "sidlc_compile() called without any SIDL files.")
    endif()
    if(NOT arg_SIFS_VAR)
        message(FATAL_ERROR "sidlc_compile() requires SIFS_VAR.")
    endif()
    if(NOT arg_OUTPUT_DIR)
        set(arg_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    file(MAKE_DIRECTORY "${arg_OUTPUT_DIR}")
    set(generated_sifs)

    foreach(sidl_file IN LISTS arg_FILES)
        get_filename_component(abs_file "${sidl_file}" ABSOLUTE)
        get_filename_component(basename "${sidl_file}" NAME_WE)
        set(out_sif "${arg_OUTPUT_DIR}/${basename}.sif")

        add_custom_command(
            OUTPUT "${out_sif}"
            COMMAND ${SIDLC_EXECUTABLE} compile -o "${out_sif}" "${abs_file}"
            DEPENDS "${abs_file}" ${SIDLC_EXECUTABLE}
            COMMENT "Compiling SIDL interface artifact: ${basename}.sif"
            VERBATIM
        )
        set_source_files_properties("${out_sif}" PROPERTIES GENERATED TRUE)
        list(APPEND generated_sifs "${out_sif}")
    endforeach()

    set(${arg_SIFS_VAR} ${generated_sifs} PARENT_SCOPE)
endfunction()

# =========================================================================
# sidlc_generate
# Usage:
#   sidlc_generate(CLIENT|SERVER|SERVER_CLIENT
#       HEADER_DIR <dir>
#       [INCLUDE_DIR <include-path>]
#       [ARCH <arch>]
#       [LANG <lang>]
#       SRCS_VAR <source_list_out_var_name>
#       HDRS_VAR <header_list_out_var_name>
#       SIFS <file.sif>...)
#
# Generates source/header bindings from compiled .sif artifacts.
# =========================================================================
function(sidlc_generate)
    set(options CLIENT SERVER SERVER_CLIENT WEAK)
    set(oneValueArgs ARCH LANG HEADER_DIR INCLUDE_DIR SRCS_VAR HDRS_VAR)
    set(multiValueArgs SIFS)
    cmake_parse_arguments(PARSE_ARGV 0 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    if(NOT arg_SIFS)
        message(FATAL_ERROR "sidlc_generate() called without any SIF files.")
    endif()
    if(NOT arg_CLIENT AND NOT arg_SERVER AND NOT arg_SERVER_CLIENT)
        message(FATAL_ERROR "sidlc_generate() requires one of CLIENT, SERVER, or SERVER_CLIENT.")
    endif()
    if(arg_CLIENT AND arg_SERVER)
        message(FATAL_ERROR "sidlc_generate() accepts only one generation mode.")
    endif()
    if(arg_CLIENT AND arg_SERVER_CLIENT)
        message(FATAL_ERROR "sidlc_generate() accepts only one generation mode.")
    endif()
    if(arg_SERVER AND arg_SERVER_CLIENT)
        message(FATAL_ERROR "sidlc_generate() accepts only one generation mode.")
    endif()
    if(NOT arg_SRCS_VAR)
        message(FATAL_ERROR "sidlc_generate() requires SRCS_VAR.")
    endif()
    if(NOT arg_HDRS_VAR)
        message(FATAL_ERROR "sidlc_generate() requires HDRS_VAR.")
    endif()
    if(NOT arg_HEADER_DIR)
        set(arg_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    if(NOT arg_ARCH)
        set(arg_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if(NOT arg_LANG)
        set(arg_LANG c)
    endif()

    if(arg_CLIENT)
        set(generate_mode client)
        set(header_suffix ".h")
        set(source_suffix ".client.c")
    elseif(arg_SERVER)
        set(generate_mode server)
        set(header_suffix ".server.h")
        set(source_suffix ".server.c")
    else()
        set(generate_mode server-client)
        set(header_suffix ".server-client.h")
        set(source_suffix ".server-client.c")
    endif()

    file(MAKE_DIRECTORY "${arg_HEADER_DIR}")
    set(generated_srcs)
    set(generated_hdrs)

    foreach(sif_file IN LISTS arg_SIFS)
        get_filename_component(abs_sif "${sif_file}" ABSOLUTE)
        get_filename_component(basename "${sif_file}" NAME_WE)

        set(out_types_hdr "${arg_HEADER_DIR}/${basename}.types.h")
        set(out_binding_hdr "${arg_HEADER_DIR}/${basename}${header_suffix}")
        set(out_binding_src "${arg_HEADER_DIR}/${basename}${source_suffix}")
        set(command_args
            generate
            -a "${arg_ARCH}"
            -l "${arg_LANG}"
            -m "${generate_mode}"
            -h "${arg_HEADER_DIR}"
            -s "${out_binding_src}"
        )
        if(arg_INCLUDE_DIR)
            list(APPEND command_args -i "${arg_INCLUDE_DIR}")
        endif()
        if(arg_WEAK)
            list(APPEND command_args --weak)
        endif()
        list(APPEND command_args "${abs_sif}")

        add_custom_command(
            OUTPUT "${out_types_hdr}" "${out_binding_hdr}" "${out_binding_src}"
            COMMAND ${SIDLC_EXECUTABLE} ${command_args}
            DEPENDS "${abs_sif}" ${SIDLC_EXECUTABLE}
            COMMENT "Generating bindings from SIDL interface artifact: ${basename}.sif"
            VERBATIM
        )
        set_source_files_properties(
            "${out_types_hdr}" "${out_binding_hdr}" "${out_binding_src}"
            PROPERTIES GENERATED TRUE
        )

        list(APPEND generated_hdrs "${out_types_hdr}" "${out_binding_hdr}")
        list(APPEND generated_srcs "${out_binding_src}")
    endforeach()

    set(${arg_SRCS_VAR} ${generated_srcs} PARENT_SCOPE)
    set(${arg_HDRS_VAR} ${generated_hdrs} PARENT_SCOPE)
endfunction()
