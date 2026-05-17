#ifndef __C_HANDLER_HH__
#define __C_HANDLER_HH__

#include <string>

struct InterfaceNode;

enum class CGenerateMode {
    Client,
    Server,
    ServerClient,
};

struct CGenerateOptions {
    CGenerateMode mode;
    std::string type_header_path;
    std::string type_header_include_path;
    std::string header_path;
    std::string source_path;
    std::string source_header_include_path;
    bool make_weak_symbols = false;
};

bool c_generate(InterfaceNode *interface, const CGenerateOptions &options);

#endif  // __C_HANDLER_HH__
