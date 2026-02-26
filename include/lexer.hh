#ifndef __LEXER_HH__
#define __LEXER_HH__

#include <string_view>

struct Token {
    enum Type : int {
        TYPE_UNKNOWN = -1,

        TYPE_ENDOFFILE = 256,
        TYPE_IDENTIFIER,
        TYPE_NUMBER,
        TYPE_STRING,

        TYPE_KWD_INTERFACE,
        TYPE_KWD_GROUP,
        TYPE_KWD_ABIREVISION,
        TYPE_KWD_STRUCT,
        TYPE_KWD_FUNCTION,
        TYPE_KWD_INOUT,
        TYPE_KWD_IN,
        TYPE_KWD_OUT,
        TYPE_KWD_BITFIELD,
        TYPE_KWD_PTR,
        TYPE_KWD_CONST,
    };

    Type type;
    std::string_view text;
    size_t line;
    size_t start_column;
};

class Lexer {
  private:
    std::string_view source;
    size_t pos;
    size_t current_line;
    size_t current_column;

    void skip_unmeaningful_string();
    void advance();

  public:
    Lexer(std::string_view source) : source(source), pos(0), current_line(1), current_column(1) {}

    Token next_token();
};

#endif  // __LEXER_HH__
