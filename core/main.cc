#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>

#include <arch_abi.hh>
#include <c_handler.hh>
#include <lang_info.hh>
#include <lexer.hh>
#include <parser.hh>

#include "config.h"

static const std::map<std::string, LangInfo> lang_infos = {
    {
        "c",
        {
            "c",
            {
                { "opaque", { "void", 0, 0 } },
                { "u8", { "uint8_t", 1, 1 } },
                { "u16", { "uint16_t", 2, 2 } },
                { "u32", { "uint32_t", 4, 4 } },
                { "u64", { "uint64_t", 8, 8 } },
                { "s8", { "int8_t", 1, 1 } },
                { "s16", { "int16_t", 2, 2 } },
                { "s32", { "int32_t", 4, 4 } },
                { "s64", { "int64_t", 8, 8 } },
                { "handle", { "StHandle", 4, 4 } },
                { "status", { "StStatus", 4, 4 } },
            },
            c_handle_option,
            c_generate,
        },
    },
};

static const std::map<std::string, ArchAbi> arch_abis = {
    { "x86_64", { "x86_64", 8, 6 } },
};

const ArchAbi *g_current_arch_abi = nullptr;
const LangInfo *g_current_lang_info = nullptr;

void print_usage(const char *argv0)
{
    std::cerr
        << "Usage: " << argv0 << " [options] <file>\n"
        << "Options:\n"
           "  -h, --help    Print this help message\n"
           "  -v, --version Print the version number\n"
           "  --arch=<arch> Set output architecture\n"
           "  --lang=<lang> Set output language\n\n"
           "Per-language options:\n"
           "  C: (--lang=c)\n"
           "    --weak                        Make weak symbols\n"
           "    --header=<path>               Output header file path (.h)\n"
           "    --user-src=<path>             Output source file path (.c)\n"
           "    --user-src-header-path=<path> Include path to be written in the generated source\n";
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string input_file_path;
    std::string arch;
    std::string lang;

    // First pass to find the language handler and handle immediate exit flags
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "sidlc version " << SIDLC_VERSION << " (" << SIDLC_GIT_HASH << ")"
                      << std::endl;
            return 0;
        } else if (arg.rfind("--lang=", 0) == 0) {
            lang = arg.substr(7);
        }
    }

    if (lang.empty()) {
        std::cerr << "Error: Language not specified" << '\n';
        return 1;
    }

    auto it_lang = lang_infos.find(lang);
    if (it_lang == lang_infos.end()) {
        std::cerr << "Error: Unknown language " << lang << '\n';
        return 1;
    }
    g_current_lang_info = &it_lang->second;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg.rfind("--arch=", 0) == 0) {
            arch = arg.substr(7);
        } else if (arg.rfind("--lang=", 0) == 0) {
            // Handled in first pass
        } else if (arg.rfind("--") == 0) {
            if (!g_current_lang_info->handle_option(arg)) {
                std::cerr << "Error: Unknown option " << arg << '\n';
                return 1;
            }
        } else if (input_file_path.empty()) {
            input_file_path = arg;
        } else {
            std::cerr << "Error: Too many input files" << '\n';
            return 1;
        }
    }

    if (arch.empty()) {
        std::cerr << "Error: Architecture not specified" << '\n';
        return 1;
    }

    auto it = arch_abis.find(arch);
    if (it == arch_abis.end()) {
        std::cerr << "Error: Unknown architecture " << arch << '\n';
        return 1;
    }
    g_current_arch_abi = &it->second;

    std::ifstream file(input_file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << input_file_path << std::endl;
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Parser parser(source);

    auto interface = parser.parse();

    if (!g_current_lang_info->generate(interface.get())) {
        return 1;
    }

    return 0;
}
