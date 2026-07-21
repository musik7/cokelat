/**
 * @file lexer_enhanced.cpp
 * @brief Enhanced JavaScript Lexer with Modern Language Support
 * 
 * Supports modern JavaScript including async/await, arrow functions,
 * destructuring, spread operators, template literals, and more.
 */

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <set>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. ENHANCED TOKEN TYPES
// ==========================================

enum class TokenTypeEnhanced {
    UNKNOWN,
    KEYWORD,
    KEYWORD_ASYNC,
    KEYWORD_AWAIT,
    KEYWORD_ARROW,  // =>
    IDENTIFIER,
    NUMBER,
    STRING,
    TEMPLATE_LITERAL,
    OPERATOR,
    OPERATOR_SPREAD,  // ...
    OPERATOR_DESTRUCTURE, // [ or ]
    PUNCTUATION,
    WHITESPACE,
    COMMENT_SINGLE,
    COMMENT_MULTI,
    REGEX,
    JSX_TAG,
    JSX_PROP
};

struct TokenEnhanced {
    TokenTypeEnhanced type;
    std::string value;
    int start;
    int end;
    int lineNumber;
    int columnNumber;

    std::string getTypeString() const {
        switch (type) {
            case TokenTypeEnhanced::KEYWORD: return "KEYWORD";
            case TokenTypeEnhanced::KEYWORD_ASYNC: return "KEYWORD_ASYNC";
            case TokenTypeEnhanced::KEYWORD_AWAIT: return "KEYWORD_AWAIT";
            case TokenTypeEnhanced::KEYWORD_ARROW: return "KEYWORD_ARROW";
            case TokenTypeEnhanced::IDENTIFIER: return "IDENTIFIER";
            case TokenTypeEnhanced::NUMBER: return "NUMBER";
            case TokenTypeEnhanced::STRING: return "STRING";
            case TokenTypeEnhanced::TEMPLATE_LITERAL: return "TEMPLATE_LITERAL";
            case TokenTypeEnhanced::OPERATOR: return "OPERATOR";
            case TokenTypeEnhanced::OPERATOR_SPREAD: return "OPERATOR_SPREAD";
            case TokenTypeEnhanced::OPERATOR_DESTRUCTURE: return "OPERATOR_DESTRUCTURE";
            case TokenTypeEnhanced::PUNCTUATION: return "PUNCTUATION";
            case TokenTypeEnhanced::WHITESPACE: return "WHITESPACE";
            case TokenTypeEnhanced::COMMENT_SINGLE: return "COMMENT_SINGLE";
            case TokenTypeEnhanced::COMMENT_MULTI: return "COMMENT_MULTI";
            case TokenTypeEnhanced::REGEX: return "REGEX";
            case TokenTypeEnhanced::JSX_TAG: return "JSX_TAG";
            case TokenTypeEnhanced::JSX_PROP: return "JSX_PROP";
            default: return "UNKNOWN";
        }
    }
};

// ==========================================
// 2. ENHANCED LEXER
// ==========================================

class LexerEnhanced {
private:
    std::string source;
    size_t cursor = 0;
    int lineNumber = 1;
    int lineStart = 0;

    // Comprehensive keyword list (ES2022+)
    static const std::set<std::string>& getKeywords() {
        static const std::set<std::string> keywords = {
            // Control flow
            "if", "else", "switch", "case", "default", "break", "continue", "return",
            // Loops
            "for", "while", "do", "in", "of",
            // Functions
            "function", "async", "await", "yield",
            // Variables
            "const", "let", "var",
            // Classes & Objects
            "class", "extends", "new", "this", "super",
            // Error handling
            "try", "catch", "finally", "throw",
            // Operators & Values
            "typeof", "instanceof", "delete", "void",
            "true", "false", "null", "undefined",
            // Imports/Exports
            "import", "export", "from", "as", "default",
            // Other ES6+
            "static", "get", "set", "constructor",
            // Global objects (common)
            "console", "window", "document", "JSON", "Math", "Array", "Object", "String", "Promise",
            // Modules
            "require", "module"
        };
        return keywords;
    }

    bool isWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    bool isAlpha(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
    }

    bool isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    bool isKeyword(const std::string& val) {
        return getKeywords().count(val) > 0;
    }

public:
    LexerEnhanced(const std::string& src) : source(src) {}

    std::vector<TokenEnhanced> tokenize() {
        std::vector<TokenEnhanced> tokens;
        size_t len = source.length();

        while (cursor < len) {
            char current = source[cursor];
            size_t startPos = cursor;
            int startLine = lineNumber;
            int startCol = cursor - lineStart;

            // Whitespace
            if (isWhitespace(current)) {
                std::string val = "";
                while (cursor < len && isWhitespace(source[cursor])) {
                    if (source[cursor] == '\n') {
                        lineNumber++;
                        lineStart = cursor + 1;
                    }
                    val += source[cursor++];
                }
                tokens.push_back({TokenTypeEnhanced::WHITESPACE, val, (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // Comments
            if (current == '/' && cursor + 1 < len) {
                if (source[cursor + 1] == '/') {
                    // Single-line comment
                    std::string val = "";
                    while (cursor < len && source[cursor] != '\n') {
                        val += source[cursor++];
                    }
                    tokens.push_back({TokenTypeEnhanced::COMMENT_SINGLE, val, (int)startPos, (int)cursor, startLine, startCol});
                    continue;
                } else if (source[cursor + 1] == '*') {
                    // Multi-line comment
                    std::string val = "";
                    val += source[cursor++];
                    val += source[cursor++];
                    while (cursor + 1 < len) {
                        if (source[cursor] == '*' && source[cursor + 1] == '/') {
                            val += source[cursor++];
                            val += source[cursor++];
                            break;
                        }
                        if (source[cursor] == '\n') {
                            lineNumber++;
                            lineStart = cursor + 1;
                        }
                        val += source[cursor++];
                    }
                    tokens.push_back({TokenTypeEnhanced::COMMENT_MULTI, val, (int)startPos, (int)cursor, startLine, startCol});
                    continue;
                }
            }

            // Template literals (backticks)
            if (current == '`') {
                std::string val = "`";
                cursor++;
                while (cursor < len && source[cursor] != '`') {
                    if (source[cursor] == '\\' && cursor + 1 < len) {
                        val += source[cursor++];
                        val += source[cursor++];
                    } else {
                        if (source[cursor] == '\n') {
                            lineNumber++;
                            lineStart = cursor + 1;
                        }
                        val += source[cursor++];
                    }
                }
                if (cursor < len) {
                    val += source[cursor++];
                }
                tokens.push_back({TokenTypeEnhanced::TEMPLATE_LITERAL, val, (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // String literals
            if (current == '"' || current == '\'') {
                char quote = current;
                std::string val = "";
                val += quote;
                cursor++;
                while (cursor < len && source[cursor] != quote) {
                    if (source[cursor] == '\\' && cursor + 1 < len) {
                        val += source[cursor++];
                        val += source[cursor++];
                    } else {
                        val += source[cursor++];
                    }
                }
                if (cursor < len) {
                    val += source[cursor++];
                }
                tokens.push_back({TokenTypeEnhanced::STRING, val, (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // Numbers
            if (isDigit(current) || (current == '.' && cursor + 1 < len && isDigit(source[cursor + 1]))) {
                std::string val = "";
                while (cursor < len && (isDigit(source[cursor]) || source[cursor] == '.' || source[cursor] == 'e' || source[cursor] == 'E')) {
                    val += source[cursor++];
                }
                tokens.push_back({TokenTypeEnhanced::NUMBER, val, (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // Identifiers and Keywords
            if (isAlpha(current)) {
                std::string val = "";
                while (cursor < len && (isAlpha(source[cursor]) || isDigit(source[cursor]))) {
                    val += source[cursor++];
                }

                TokenTypeEnhanced tokenType = TokenTypeEnhanced::IDENTIFIER;
                if (val == "async") tokenType = TokenTypeEnhanced::KEYWORD_ASYNC;
                else if (val == "await") tokenType = TokenTypeEnhanced::KEYWORD_AWAIT;
                else if (isKeyword(val)) tokenType = TokenTypeEnhanced::KEYWORD;

                tokens.push_back({tokenType, val, (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // Spread operator (...)
            if (current == '.' && cursor + 2 < len && source[cursor + 1] == '.' && source[cursor + 2] == '.') {
                cursor += 3;
                tokens.push_back({TokenTypeEnhanced::OPERATOR_SPREAD, "...", (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // Arrow function (=>)
            if (current == '=' && cursor + 1 < len && source[cursor + 1] == '>') {
                cursor += 2;
                tokens.push_back({TokenTypeEnhanced::KEYWORD_ARROW, "=>", (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // Operators (including multi-char)
            if (std::string("+-*/%=!&|^<>").find(current) != std::string::npos) {
                std::string val = "";
                val += current;
                cursor++;
                if (cursor < len && std::string("=&|<>").find(source[cursor]) != std::string::npos) {
                    val += source[cursor++];
                }
                tokens.push_back({TokenTypeEnhanced::OPERATOR, val, (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // Destructuring brackets
            if (current == '[' || current == ']') {
                std::string val = "";
                val += current;
                cursor++;
                tokens.push_back({TokenTypeEnhanced::OPERATOR_DESTRUCTURE, val, (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // Punctuation
            if (std::string("(){},:;.").find(current) != std::string::npos) {
                std::string val = "";
                val += current;
                cursor++;
                tokens.push_back({TokenTypeEnhanced::PUNCTUATION, val, (int)startPos, (int)cursor, startLine, startCol});
                continue;
            }

            // Unknown
            std::string val = "";
            val += current;
            cursor++;
            tokens.push_back({TokenTypeEnhanced::UNKNOWN, val, (int)startPos, (int)cursor, startLine, startCol});
        }

        return tokens;
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(lexer_enhanced_module) {
    class_<TokenEnhanced>("TokenEnhanced")
        .property("typeStr", &TokenEnhanced::getTypeString)
        .property("value", &TokenEnhanced::value)
        .property("start", &TokenEnhanced::start)
        .property("end", &TokenEnhanced::end)
        .property("lineNumber", &TokenEnhanced::lineNumber)
        .property("columnNumber", &TokenEnhanced::columnNumber);

    register_vector<TokenEnhanced>("VectorTokenEnhanced");

    class_<LexerEnhanced>("LexerEnhanced")
        .constructor<std::string>()
        .function("tokenize", &LexerEnhanced::tokenize);

    enum_<TokenTypeEnhanced>("TokenTypeEnhanced")
        .value("KEYWORD", TokenTypeEnhanced::KEYWORD)
        .value("KEYWORD_ASYNC", TokenTypeEnhanced::KEYWORD_ASYNC)
        .value("KEYWORD_AWAIT", TokenTypeEnhanced::KEYWORD_AWAIT)
        .value("KEYWORD_ARROW", TokenTypeEnhanced::KEYWORD_ARROW)
        .value("IDENTIFIER", TokenTypeEnhanced::IDENTIFIER)
        .value("OPERATOR_SPREAD", TokenTypeEnhanced::OPERATOR_SPREAD)
        .value("TEMPLATE_LITERAL", TokenTypeEnhanced::TEMPLATE_LITERAL)
        .value("COMMENT_SINGLE", TokenTypeEnhanced::COMMENT_SINGLE)
        .value("COMMENT_MULTI", TokenTypeEnhanced::COMMENT_MULTI);
}
#endif
