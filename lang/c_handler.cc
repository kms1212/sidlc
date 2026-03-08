#include <c_handler.hh>

#include <fstream>
#include <iostream>
#include <string>

#include <ast.hh>
#include <c_header_generator.hh>
#include <c_source_generator.hh>

static std::string header_path;
static std::string user_src_path;
static std::string user_src_header_path;
static bool make_weak_symbols = false;

bool c_handle_option(const std::string &arg)
{
    if (arg.rfind("--weak", 0) == 0) {
        make_weak_symbols = true;
        return true;
    } else if (arg.rfind("--header=", 0) == 0) {
        header_path = arg.substr(9);
        return true;
    } else if (arg.rfind("--user-src=", 0) == 0) {
        user_src_path = arg.substr(11);
        return true;
    } else if (arg.rfind("--user-src-header-path=", 0) == 0) {
        user_src_header_path = arg.substr(23);
        return true;
    }
    return false;
}

bool c_generate(InterfaceNode *interface)
{
    if (!header_path.empty()) {
        std::ofstream header_file(header_path);
        if (!header_file.is_open()) {
            std::cerr << "Error: Could not open file " << header_path << std::endl;
            return false;
        }
        CHeaderGenerator header_gen(header_file);
        interface->accept(header_gen);
    }

    if (user_src_header_path.empty()) {
        user_src_header_path = header_path.substr(header_path.rfind("/") + 1);
    }

    if (!user_src_path.empty()) {
        std::ofstream user_src_file(user_src_path);
        if (!user_src_file.is_open()) {
            std::cerr << "Error: Could not open file " << user_src_path << std::endl;
            return false;
        }
        CSourceGenerator source_gen(user_src_file, user_src_header_path, make_weak_symbols);
        interface->accept(source_gen);
    }

    return true;
}
