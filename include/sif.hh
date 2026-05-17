#ifndef __SIF_HH__
#define __SIF_HH__

#include <memory>
#include <iosfwd>
#include <vector>

struct InterfaceNode;

void sif_write(
    std::ostream &out,
    const InterfaceNode &interface,
    const std::vector<const InterfaceNode *> &extension_interfaces
);
std::unique_ptr<InterfaceNode> sif_read(std::istream &in);
void sif_decompile(std::ostream &out, const InterfaceNode &interface);

#endif  // __SIF_HH__
