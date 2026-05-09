#include <c_handler.hh>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <ast.hh>
#include <c_header_generator.hh>
#include <c_user_source_generator.hh>
#include <c_module_source_generator.hh>

static std::string type_header_path;
static std::string client_header_path;
static std::string client_header_type_path;
static std::string server_header_path;
static std::string server_header_type_path;
static std::string server_client_header_path;
static std::string server_client_header_type_path;
static std::string client_src_path;
static std::string client_src_header_path;
static std::string server_src_path;
static std::string server_src_header_path;
static std::string server_client_src_path;
static std::string server_client_src_header_path;
static bool make_weak_symbols = false;

static std::string path_basename(const std::string &path)
{
    size_t slash = path.find_last_of("/\\");

    if (slash == std::string::npos) {
        return path;
    }

    return path.substr(slash + 1);
}

static std::string derive_include_path(const std::string &source_path, const std::string &header_path)
{
    if (header_path.empty()) {
        return {};
    }

    if (source_path.empty()) {
        return path_basename(header_path);
    }

    std::filesystem::path source_dir = std::filesystem::path(source_path).parent_path();
    std::filesystem::path relative = std::filesystem::path(header_path).lexically_relative(source_dir);

    if (relative.empty()) {
        return path_basename(header_path);
    }

    return relative.generic_string();
}

bool c_handle_option(const std::string &arg)
{
    if (arg.rfind("--weak", 0) == 0) {
        make_weak_symbols = true;
        return true;
    } else if (arg.rfind("--type-header=", 0) == 0) {
        type_header_path = arg.substr(14);
        return true;
    } else if (arg.rfind("--client-header=", 0) == 0) {
        client_header_path = arg.substr(16);
        return true;
    } else if (arg.rfind("--client-header-type-path=", 0) == 0) {
        client_header_type_path = arg.substr(26);
        return true;
    } else if (arg.rfind("--server-header=", 0) == 0) {
        server_header_path = arg.substr(16);
        return true;
    } else if (arg.rfind("--server-header-type-path=", 0) == 0) {
        server_header_type_path = arg.substr(26);
        return true;
    } else if (arg.rfind("--server-client-header=", 0) == 0) {
        server_client_header_path = arg.substr(23);
        return true;
    } else if (arg.rfind("--server-client-header-type-path=", 0) == 0) {
        server_client_header_type_path = arg.substr(33);
        return true;
    } else if (arg.rfind("--client-src=", 0) == 0) {
        client_src_path = arg.substr(13);
        return true;
    } else if (arg.rfind("--client-src-header-path=", 0) == 0) {
        client_src_header_path = arg.substr(25);
        return true;
    } else if (arg.rfind("--server-src=", 0) == 0) {
        server_src_path = arg.substr(13);
        return true;
    } else if (arg.rfind("--server-src-header-path=", 0) == 0) {
        server_src_header_path = arg.substr(25);
        return true;
    } else if (arg.rfind("--server-client-src=", 0) == 0) {
        server_client_src_path = arg.substr(20);
        return true;
    } else if (arg.rfind("--server-client-src-header-path=", 0) == 0) {
        server_client_src_header_path = arg.substr(32);
        return true;
    }
    return false;
}

static bool generate_client_header(
    InterfaceNode *interface,
    const std::string &header_path,
    const std::string &header_type_path,
    CHeaderGenerator::Mode mode
)
{
    std::string type_header_include_path;

    if (type_header_path.empty()) {
        std::cerr << "Error: client header requires --type-header" << std::endl;
        return false;
    }

    if (!header_type_path.empty()) {
        type_header_include_path = header_type_path;
    } else {
        type_header_include_path = derive_include_path(header_path, type_header_path);
    }

    std::ofstream header_file(header_path);
    if (!header_file.is_open()) {
        std::cerr << "Error: Could not open file " << header_path << std::endl;
        return false;
    }

    CHeaderGenerator header_gen(header_file, mode, type_header_include_path);
    interface->accept(header_gen);

    return true;
}

static bool generate_client_source(
    InterfaceNode *interface, const std::string &source_path, const std::string &header_path
)
{
    std::string source_header_path = header_path;

    if (source_header_path.empty()) {
        std::cerr << "Error: Could not determine client source header path" << std::endl;
        return false;
    }

    std::ofstream source_file(source_path);
    if (!source_file.is_open()) {
        std::cerr << "Error: Could not open file " << source_path << std::endl;
        return false;
    }

    CUserSourceGenerator source_gen(source_file, source_header_path, make_weak_symbols);
    interface->accept(source_gen);

    return true;
}

bool c_generate_user(InterfaceNode *interface)
{
    if (!type_header_path.empty()) {
        std::ofstream header_file(type_header_path);
        if (!header_file.is_open()) {
            std::cerr << "Error: Could not open file " << type_header_path << std::endl;
            return false;
        }
        CHeaderGenerator header_gen(header_file, CHeaderGenerator::Mode::TYPE);
        interface->accept(header_gen);
    }

    if (!client_header_path.empty()) {
        if (!generate_client_header(
                interface,
                client_header_path,
                client_header_type_path,
                CHeaderGenerator::Mode::CLIENT
            )) {
            return false;
        }
    }

    if (!server_client_header_path.empty()) {
        if (!generate_client_header(
                interface,
                server_client_header_path,
                server_client_header_type_path,
                CHeaderGenerator::Mode::SERVER_CLIENT
            )) {
            return false;
        }
    }

    if (client_src_header_path.empty() && !client_header_path.empty()) {
        client_src_header_path = derive_include_path(client_src_path, client_header_path);
    }

    if (!client_src_path.empty()) {
        if (!generate_client_source(interface, client_src_path, client_src_header_path)) {
            return false;
        }
    }

    if (server_client_src_header_path.empty() && !server_client_header_path.empty()) {
        server_client_src_header_path =
            derive_include_path(server_client_src_path, server_client_header_path);
    }

    if (!server_client_src_path.empty()) {
        if (!generate_client_source(
                interface,
                server_client_src_path,
                server_client_src_header_path
            )) {
            return false;
        }
    }

    return true;
}

bool c_generate_module(InterfaceNode *interface)
{
    if (!server_header_path.empty()) {
        std::string server_header_type_header_path;

        if (type_header_path.empty()) {
            std::cerr << "Error: --server-header requires --type-header" << std::endl;
            return false;
        }

        if (!server_header_type_path.empty()) {
            server_header_type_header_path = server_header_type_path;
        } else {
            server_header_type_header_path =
                derive_include_path(server_header_path, type_header_path);
        }

        std::ofstream header_file(server_header_path);
        if (!header_file.is_open()) {
            std::cerr << "Error: Could not open file " << server_header_path << std::endl;
            return false;
        }
        CHeaderGenerator header_gen(
            header_file,
            CHeaderGenerator::Mode::SERVER,
            server_header_type_header_path
        );
        interface->accept(header_gen);
    }

    if (server_src_header_path.empty() && !server_header_path.empty()) {
        server_src_header_path = derive_include_path(server_src_path, server_header_path);
    }

    if (!server_src_path.empty()) {
        if (server_src_header_path.empty()) {
            std::cerr << "Error: Could not determine server source header path" << std::endl;
            return false;
        }

        std::ofstream server_src_file(server_src_path);
        if (!server_src_file.is_open()) {
            std::cerr << "Error: Could not open file " << server_src_path << std::endl;
            return false;
        }
        CModuleSourceGenerator source_gen(server_src_file, server_src_header_path);
        interface->accept(source_gen);
    }

    return true;
}
