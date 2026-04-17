#ifndef __C_HANDLER_HH__
#define __C_HANDLER_HH__

#include <string>

struct InterfaceNode;

bool c_handle_option(const std::string &arg);
bool c_generate_user(InterfaceNode *interface);
bool c_generate_module(InterfaceNode *interface);

#endif  // __C_HANDLER_HH__
