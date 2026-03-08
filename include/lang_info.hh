#ifndef __LANG_INFO_HH__
#define __LANG_INFO_HH__

#include <map>
#include <string>

struct LangTypeInfo {
    std::string lang_name;
    size_t size;
    size_t alignment;
};

struct InterfaceNode;

struct LangInfo {
    std::string name;
    std::map<std::string, LangTypeInfo> type_infos;
    bool (*handle_option)(const std::string &arg);
    bool (*generate)(InterfaceNode *interface);
};

extern const LangInfo *g_current_lang_info;

#endif  // __LANG_INFO_HH__
