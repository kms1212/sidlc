#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <c_header_generator.hh>
#include <c_source_generator.hh>
#include <lexer.hh>
#include <parser.hh>

#include "config.h"

void print_usage(const char *argv0)
{
    std::cerr << "Usage: " << argv0 << " [options] <file>" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -h, --help  Print this help message" << std::endl;
    std::cerr << "  -v, --version  Print the version number" << std::endl;
    std::cerr << "  --header=<path>                 Output C header file path (.h)" << std::endl;
    std::cerr << "  --user-src=<path>               Output C source file path (.c)" << std::endl;
    std::cerr
        << "  --user-src-header-path=<path>   Include path to be written in the generated C source"
        << std::endl;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string input_file_path;
    std::string header_path;
    std::string user_src_path;
    std::string user_src_header_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << "sidlc version " << SIDLC_VERSION << " (" << SIDLC_GIT_HASH << ")"
                      << std::endl;
            return 0;
        }
        if (arg.rfind("--header=", 0) == 0) {
            header_path = arg.substr(9);
            continue;
        }
        if (arg.rfind("--user-src=", 0) == 0) {
            user_src_path = arg.substr(11);
            continue;
        }
        if (arg.rfind("--user-src-header-path=", 0) == 0) {
            user_src_header_path = arg.substr(23);
            continue;
        }
        if (arg.rfind("--") == 0) {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            return 1;
        }
        if (input_file_path.empty()) {
            input_file_path = arg;
            continue;
        }
        std::cerr << "Error: Too many input files" << std::endl;
        return 1;
    }

    std::ifstream file(input_file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << input_file_path << std::endl;
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Parser parser(source);

    auto interface = parser.parse();

    if (!header_path.empty()) {
        std::ofstream header_file(header_path);
        if (!header_file.is_open()) {
            std::cerr << "Error: Could not open file " << header_path << std::endl;
            return 1;
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
            return 1;
        }
        CSourceGenerator source_gen(user_src_file, user_src_header_path);
        interface->accept(source_gen);
    }

    return 0;
}
