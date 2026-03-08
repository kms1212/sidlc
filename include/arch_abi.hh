#ifndef __ARCH_ABI_HH__
#define __ARCH_ABI_HH__

#include <string>

struct ArchAbi {
    std::string name;
    size_t pointer_size;
    long max_reg_args;
};

extern const ArchAbi *g_current_arch_abi;

#endif  // __ARCH_ABI_HH__
