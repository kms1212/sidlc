#include <lexer.hh>

#include <cctype>

void Lexer::advance()
{
    if (source[pos] == '\n') {
        current_line++;
        current_column = 1;
    } else {
        current_column++;
    }
    pos++;
}

void Lexer::skip_unmeaningful_string()
{
    while (pos < source.length()) {
        char c = source[pos];
        if (std::isspace(c) || c == '\n') {
            advance();
        } else if (c == '/') {
            advance();
            if (pos < source.length() && source[pos] == '/') {
                while (pos < source.length() && source[pos] != '\n') {
                    advance();
                }
            } else if (pos < source.length() && source[pos] == '*') {
                advance();
                while (pos < source.length()) {
                    if (pos + 1 < source.length() && source[pos] == '*' && source[pos + 1] == '/') {
                        advance();
                        advance();
                        break;
                    }
                    advance();
                }
            }
        } else {
            break;
        }
    }
}

Token Lexer::next_token()
{
    skip_unmeaningful_string();

    if (pos >= source.length()) {
        return {
            Token::TYPE_ENDOFFILE,
            "",
            current_line,
            current_column,
        };
    }

    char c = source[pos];
    size_t start_column = current_column;

    // identifier or keyword
    if (std::isalpha(c) || c == '_') {
        size_t start_pos = pos;
        while (pos < source.length() && (std::isalnum(source[pos]) || source[pos] == '_')) {
            advance();
        }
        std::string_view text = source.substr(start_pos, pos - start_pos);

        Token::Type type = Token::TYPE_IDENTIFIER;
        if (text == "interface") {
            type = Token::TYPE_KWD_INTERFACE;
        } else if (text == "group") {
            type = Token::TYPE_KWD_GROUP;
        } else if (text == "abirevision") {
            type = Token::TYPE_KWD_ABIREVISION;
        } else if (text == "struct") {
            type = Token::TYPE_KWD_STRUCT;
        } else if (text == "function") {
            type = Token::TYPE_KWD_FUNCTION;
        } else if (text == "inout") {
            type = Token::TYPE_KWD_INOUT;
        } else if (text == "in") {
            type = Token::TYPE_KWD_IN;
        } else if (text == "out") {
            type = Token::TYPE_KWD_OUT;
        } else if (text == "bitfield") {
            type = Token::TYPE_KWD_BITFIELD;
        } else if (text == "ptr") {
            type = Token::TYPE_KWD_PTR;
        } else if (text == "const") {
            type = Token::TYPE_KWD_CONST;
        }

        return {type, text, current_line, start_column};
    }

    if (c == '0') {
        advance();

        if (pos < source.length() && (source[pos] == 'x' || source[pos] == 'X')) {
            // hexadecimal
            advance();
            size_t start_pos = pos;
            while (pos < source.length() && std::isxdigit(source[pos])) {
                advance();
            }
            std::string_view text = source.substr(start_pos, pos - start_pos);
            return {Token::TYPE_NUMBER, text, current_line, start_column};
        } else if (pos < source.length() && (source[pos] == 'b' || source[pos] == 'B')) {
            // binary
            advance();
            size_t start_pos = pos;
            while (pos < source.length() && (source[pos] == '0' || source[pos] == '1')) {
                advance();
            }
            std::string_view text = source.substr(start_pos, pos - start_pos);
            return {Token::TYPE_NUMBER, text, current_line, start_column};
        } else if (pos < source.length() && std::isdigit(source[pos])) {
            // octal
            size_t start_pos = pos;
            while (pos < source.length() && std::isdigit(source[pos])) {
                advance();
            }
            std::string_view text = source.substr(start_pos, pos - start_pos);
            return {Token::TYPE_NUMBER, text, current_line, start_column};
        } else {
            // 0
            std::string_view text = source.substr(pos - 1, 1);
            return {Token::TYPE_NUMBER, text, current_line, start_column};
        }
    }

    if (std::isdigit(c)) {
        size_t start_pos = pos;
        while (pos < source.length() && std::isdigit(source[pos])) {
            advance();
        }
        std::string_view text = source.substr(start_pos, pos - start_pos);
        return {Token::TYPE_NUMBER, text, current_line, start_column};
    }

    if (c == '"') {
        advance();
        size_t start_pos = pos;
        while (pos < source.length() && source[pos] != '"') {
            advance();
        }
        std::string_view text = source.substr(start_pos - 1, pos - start_pos + 2);
        advance();
        return {Token::TYPE_STRING, text, current_line, start_column};
    }

    Token::Type token_type = Token::Type(c);
    size_t op_len = 1;
    switch (c) {
    case '@':
    case '{':
    case '}':
    case '(':
    case ')':
    case '[':
    case ']':
    case '<':
    case '>':
    case ':':
    case ';':
    case ',':
    case '.':
        advance();
        break;
    default:
        advance();
        token_type = Token::TYPE_UNKNOWN;
        break;
    }

    std::string_view text = source.substr(pos - op_len, op_len);

    return {token_type, text, current_line, start_column};
}
