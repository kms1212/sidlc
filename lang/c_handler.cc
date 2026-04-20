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
static std::string user_header_path;
static std::string user_header_type_path;
static std::string module_header_path;
static std::string module_header_type_path;
static std::string user_src_path;
static std::string user_src_header_path;
static std::string module_src_path;
static std::string module_src_header_path;
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
    } else if (arg.rfind("--user-header=", 0) == 0) {
        user_header_path = arg.substr(14);
        return true;
    } else if (arg.rfind("--user-header-type-path=", 0) == 0) {
        user_header_type_path = arg.substr(24);
        return true;
    } else if (arg.rfind("--module-header=", 0) == 0) {
        module_header_path = arg.substr(16);
        return true;
    } else if (arg.rfind("--module-header-type-path=", 0) == 0) {
        module_header_type_path = arg.substr(26);
        return true;
    } else if (arg.rfind("--user-src=", 0) == 0) {
        user_src_path = arg.substr(11);
        return true;
    } else if (arg.rfind("--user-src-header-path=", 0) == 0) {
        user_src_header_path = arg.substr(23);
        return true;
    } else if (arg.rfind("--module-src=", 0) == 0) {
        module_src_path = arg.substr(13);
        return true;
    } else if (arg.rfind("--module-src-header-path=", 0) == 0) {
        module_src_header_path = arg.substr(25);
        return true;
    }
    return false;
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

    if (!user_header_path.empty()) {
        std::string user_header_type_header_path;

        if (type_header_path.empty()) {
            std::cerr << "Error: --user-header requires --type-header" << std::endl;
            return false;
        }

        if (!user_header_type_path.empty()) {
            user_header_type_header_path = user_header_type_path;
        } else {
            user_header_type_header_path = derive_include_path(user_header_path, type_header_path);
        }

        std::ofstream header_file(user_header_path);
        if (!header_file.is_open()) {
            std::cerr << "Error: Could not open file " << user_header_path << std::endl;
            return false;
        }
        CHeaderGenerator header_gen(
            header_file,
            CHeaderGenerator::Mode::USER,
            user_header_type_header_path
        );
        interface->accept(header_gen);
    }

    if (user_src_header_path.empty() && !user_header_path.empty()) {
        user_src_header_path = derive_include_path(user_src_path, user_header_path);
    }

    if (!user_src_path.empty()) {
        if (user_src_header_path.empty()) {
            std::cerr << "Error: Could not determine user source header path" << std::endl;
            return false;
        }

        std::ofstream user_src_file(user_src_path);
        if (!user_src_file.is_open()) {
            std::cerr << "Error: Could not open file " << user_src_path << std::endl;
            return false;
        }
        CUserSourceGenerator source_gen(user_src_file, user_src_header_path, make_weak_symbols);
        interface->accept(source_gen);
    }

    return true;
}

bool c_generate_module(InterfaceNode *interface)
{
    if (!module_header_path.empty()) {
        std::string module_header_type_header_path;

        if (type_header_path.empty()) {
            std::cerr << "Error: --module-header requires --type-header" << std::endl;
            return false;
        }

        if (!module_header_type_path.empty()) {
            module_header_type_header_path = module_header_type_path;
        } else {
            module_header_type_header_path =
                derive_include_path(module_header_path, type_header_path);
        }

        std::ofstream header_file(module_header_path);
        if (!header_file.is_open()) {
            std::cerr << "Error: Could not open file " << module_header_path << std::endl;
            return false;
        }
        CHeaderGenerator header_gen(
            header_file,
            CHeaderGenerator::Mode::MODULE,
            module_header_type_header_path
        );
        interface->accept(header_gen);
    }

    if (module_src_header_path.empty() && !module_header_path.empty()) {
        module_src_header_path = derive_include_path(module_src_path, module_header_path);
    }

    if (!module_src_path.empty()) {
        if (module_src_header_path.empty()) {
            std::cerr << "Error: Could not determine module source header path" << std::endl;
            return false;
        }

        std::ofstream module_src_file(module_src_path);
        if (!module_src_file.is_open()) {
            std::cerr << "Error: Could not open file " << module_src_path << std::endl;
            return false;
        }
        CModuleSourceGenerator source_gen(module_src_file, module_src_header_path);
        interface->accept(source_gen);
    }

    return true;
}
