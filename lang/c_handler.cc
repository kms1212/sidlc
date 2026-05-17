#include <c_handler.hh>

#include <fstream>
#include <iostream>
#include <string>

#include <ast.hh>
#include <c_header_generator.hh>
#include <c_module_source_generator.hh>
#include <c_user_source_generator.hh>

static bool require_path(const char *field_name, const std::string &path)
{
    if (!path.empty()) {
        return true;
    }

    std::cerr << "Error: C generator requires " << field_name << '\n';
    return false;
}

static bool generate_type_header(InterfaceNode *interface, const std::string &header_path)
{
    std::ofstream header_file(header_path);
    if (!header_file.is_open()) {
        std::cerr << "Error: could not open file " << header_path << '\n';
        return false;
    }

    CHeaderGenerator header_gen(header_file, CHeaderGenerator::Mode::TYPE);
    interface->accept(header_gen);

    return true;
}

static bool generate_header(
    InterfaceNode *interface,
    const std::string &header_path,
    const std::string &type_header_include_path,
    CHeaderGenerator::Mode mode
)
{
    std::ofstream header_file(header_path);
    if (!header_file.is_open()) {
        std::cerr << "Error: could not open file " << header_path << '\n';
        return false;
    }

    CHeaderGenerator header_gen(header_file, mode, type_header_include_path);
    interface->accept(header_gen);

    return true;
}

static bool generate_user_source(
    InterfaceNode *interface,
    const std::string &source_path,
    const std::string &header_include_path,
    bool make_weak_symbols,
    bool emit_handle_binding
)
{
    std::ofstream source_file(source_path);
    if (!source_file.is_open()) {
        std::cerr << "Error: could not open file " << source_path << '\n';
        return false;
    }

    CUserSourceGenerator source_gen(
        source_file,
        header_include_path,
        make_weak_symbols,
        emit_handle_binding
    );
    interface->accept(source_gen);

    return true;
}

static bool generate_module_source(
    InterfaceNode *interface,
    const std::string &source_path,
    const std::string &header_include_path
)
{
    std::ofstream source_file(source_path);
    if (!source_file.is_open()) {
        std::cerr << "Error: could not open file " << source_path << '\n';
        return false;
    }

    CModuleSourceGenerator source_gen(source_file, header_include_path);
    interface->accept(source_gen);

    return true;
}

bool c_generate(InterfaceNode *interface, const CGenerateOptions &options)
{
    if (!require_path("type_header_path", options.type_header_path) ||
        !require_path("type_header_include_path", options.type_header_include_path) ||
        !require_path("header_path", options.header_path) ||
        !require_path("source_path", options.source_path) ||
        !require_path("source_header_include_path", options.source_header_include_path)) {
        return false;
    }

    if (!generate_type_header(interface, options.type_header_path)) {
        return false;
    }

    CHeaderGenerator::Mode header_mode;

    switch (options.mode) {
    case CGenerateMode::Client:
        header_mode = CHeaderGenerator::Mode::CLIENT;
        break;
    case CGenerateMode::Server:
        header_mode = CHeaderGenerator::Mode::SERVER;
        break;
    case CGenerateMode::ServerClient:
        header_mode = CHeaderGenerator::Mode::SERVER_CLIENT;
        break;
    }

    if (!generate_header(
            interface,
            options.header_path,
            options.type_header_include_path,
            header_mode
        )) {
        return false;
    }

    if (options.mode == CGenerateMode::Server) {
        return generate_module_source(
            interface,
            options.source_path,
            options.source_header_include_path
        );
    }

    return generate_user_source(
        interface,
        options.source_path,
        options.source_header_include_path,
        options.make_weak_symbols,
        options.mode == CGenerateMode::Client
    );
}
