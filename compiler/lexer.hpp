#ifndef __LEXER_HPP
#define __LEXER_HPP

#include "clay.hpp"

namespace clay {

//
// Token
//

enum TokenKind {
    T_NONE,
    T_UOPSTRING,
    T_OPSTRING,
    T_SYMBOL,
    T_KEYWORD,
    T_IDENTIFIER,
    T_OPIDENTIFIER,
    T_STRING_LITERAL,
    T_CHAR_LITERAL,
    T_INT_LITERAL,
    T_FLOAT_LITERAL,
    T_STATIC_INDEX,
    T_SPACE,
    T_LINE_COMMENT,
    T_BLOCK_COMMENT,
    T_LLVM,
    T_DOC_START,
    T_DOC_PROPERTY,
    T_DOC_TEXT,
    T_DOC_END
};

struct Token {
    Location location;
    llvm::SmallString<16> str;
    TokenKind tokenKind;
    Token() : tokenKind(T_NONE) {}
    explicit Token(TokenKind tokenKind) : tokenKind(tokenKind) {}
    explicit Token(TokenKind tokenKind, llvm::StringRef str) : str(str), tokenKind(tokenKind) {}
};

void tokenize(SourcePtr source, vector<Token> &tokens);

void tokenize(SourcePtr source, unsigned offset, size_t length,
              vector<Token> &tokens);

bool isSpace(char c);

}

#endif // __LEXER_HPP
