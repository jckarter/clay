#include "clay.hpp"
#include <set>
#include <cstdio>
#include <cassert>

static void initLexer(SourcePtr s);
static void cleanupLexer();
static bool nextToken(TokenPtr &x);

void tokenize(SourcePtr source, vector<TokenPtr> &tokens) {
    initLexer(source);
    TokenPtr token;
    while (nextToken(token)) {
        switch (token->tokenKind) {
        case T_SPACE :
        case T_LINE_COMMENT :
        case T_BLOCK_COMMENT :
            break;
        default :
            tokens.push_back(token);
        }
    }
    cleanupLexer();
}

static Source *lexerSource;
static char *begin;
static char *ptr;
static char *end;
static char *maxPtr;

static void initLexer(SourcePtr source) {
    lexerSource = source.ptr();
    begin = source->data;
    end = begin + source->size;
    ptr = begin;
}

static void cleanupLexer() {
    lexerSource = NULL;
    begin = ptr = end = maxPtr = NULL;
}

static char *save() { return ptr; }
static void restore(char *p) { ptr = p; }

static bool next(char &x) {
    if (ptr == end) return false;
    if (ptr > maxPtr) maxPtr = ptr;
    x = *(ptr++);
    return true;
}

static bool str(const char *s) {
    while (*s) {
        char x;
        if (!next(x)) return false;
        if (*s != x) return false;
        ++s;
    }
    return true;
}



//
// keywords and identifiers
//

static bool identChar1(char &x) {
    if (!next(x)) return false;
    if ((x >= 'a') && (x <= 'z')) return true;
    if ((x >= 'A') && (x <= 'Z')) return true;
    if (x == '_') return true;
    return false;
}

static bool identChar2(char &x) {
    if (!next(x)) return false;
    if ((x >= 'a') && (x <= 'z')) return true;
    if ((x >= 'A') && (x <= 'Z')) return true;
    if ((x >= '0') && (x <= '9')) return true;
    if (x == '_') return true;
    if (x == '?') return true;
    return false;
}

static bool identStr(string &x) {
    char c;
    if (!identChar1(c)) return false;
    x.clear();
    x.push_back(c);
    while (true) {
        char *p = save();
        if (!identChar2(c)) {
            restore(p);
            break;
        }
        x.push_back(c);
    }
    return true;
}

static std::set<string> *keywords = NULL;

static void initKeywords() {
    const char *s[] =
        {"import", "export", "record", "overloadable", "overload",
         "external", "static", "var", "ref", "and", "or", "not",
         "if", "else", "goto", "return", "returnref", "while",
         "break", "continue", "for", "in", "true", "false", NULL};
    keywords = new std::set<string>();
    for (const char **p = s; *p; ++p)
        keywords->insert(*p);
}

static bool keywordIdentifier(TokenPtr &x) {
    string s;
    if (!identStr(s)) return false;
    if (!keywords) initKeywords();
    if (keywords->find(s) != keywords->end())
        x = new Token(T_KEYWORD, s);
    else
        x = new Token(T_IDENTIFIER, s);
    return true;
}



//
// symbols
//

static const char *symbols[] = {
    "==", "!=", "<=", ">=",
    "<", ">",
    "+", "-", "*", "/", "%", "=", "&", "^", "|",
    "(", ")", "[", "]", "{", "}",
    ":", ";", ",", ".",
    NULL
};

static bool symbol(TokenPtr &x) {
    const char **s = symbols;
    char *p = save();
    while (*s) {
        restore(p);
        if (str(*s)) {
            x = new Token(T_SYMBOL, *s);
            return true;
        }
        ++s;
    }
    return false;
}



//
// hex, octal, and decimal digits
//

static bool hexDigit(int &x) {
    char c;
    if (!next(c)) return false;
    if ((c >= '0') && (c <= '9')) {
        x = c - '0';
        return true;
    }
    if ((c >= 'a') && (c <= 'f')) {
        x = c - 'a' + 10;
        return true;
    }
    if ((c >= 'A') && (c <= 'F')) {
        x = c - 'A' + 10;
        return true;
    }
    return false;
}

static bool octalDigit(int &x) {
    char c;
    if (!next(c) || (c < '0') || (c > '7'))
        return false;
    x = c - '0';
    return true;
}

static bool decimalDigit(int &x) {
    char c;
    if (!next(c) || (c < '0') || (c > '9'))
        return false;
    x = c - '0';
    return true;
}



//
// characters and strings
//

static bool hexEscapeChar(char &x) {
    int digit1, digit2;
    if (!hexDigit(digit1)) return false;
    if (!hexDigit(digit2)) return false;
    x = (char)(digit1*16 + digit2);
    return true;
}

static bool octalEscapeChar(char c, char &x) {
    int i = c - '0';
    int j;
    char *p = save();
    if (!octalDigit(j)) {
        restore(p);
        x = (char)i;
        return true;
    }
    i = i*8 + j;
    p = save();
    if (!octalDigit(j)) {
        restore(p);
        x = (char)i;
        return true;
    }
    i = i*8 + j;
    x = (char)i;
    return true;
}

static bool escapeChar(char &x) {
    char c;
    if (!next(c)) return false;
    if (c != '\\') return false;
    if (!next(c)) return false;
    switch (c) {
    case 'n' : x = '\n'; return true;
    case 'r' : x = '\r'; return true;
    case 't' : x = '\t'; return true;
    case 'a' : x = '\a'; return true;
    case 'b' : x = '\b'; return true;
    case 'f' : x = '\f'; return true;
    case 'v' : x = '\v'; return true;
    case '\\' :
    case '\'' :
    case '"' :
        x = c; return true;
    case 'x' :
        return hexEscapeChar(x);
    }
    if ((c >= '0') && (c <= '7'))
        return octalEscapeChar(c, x);
    return false;
}

static bool oneChar(char &x) {
    char *p = save();
    if (escapeChar(x)) return true;
    restore(p);
    if (!next(x)) return false;
    if (x == '\\') return false;
    return true;
}

static bool charToken(TokenPtr &x) {
    char c;
    if (!next(c) || (c != '\'')) return false;
    char *p = save();
    if (next(c) && (c == '\'')) return false;
    restore(p);
    char v;
    if (!oneChar(v)) return false;
    if (!next(c) || (c != '\'')) return false;
    x = new Token(T_CHAR_LITERAL, string(1, v));
    return true;
}

static bool stringToken(TokenPtr &x) {
    char c;
    if (!next(c) || (c != '"')) return false;
    string s;
    while (true) {
        char *p = save();
        if (next(c) && (c == '"'))
            break;
        restore(p);
        if (!oneChar(c)) return false;
        s.push_back(c);
    }
    x = new Token(T_STRING_LITERAL, s);
    return true;
}



//
// integer tokens
//

static bool hexInt() {
    if (!str("0x")) return false;
    int x;
    if (!hexDigit(x)) return false;
    while (true) {
        char *p = save();
        if (!hexDigit(x)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool decimalInt() {
    int x;
    if (!decimalDigit(x)) return false;
    while (true) {
        char *p = save();
        if (!decimalDigit(x)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool sign() {
    char c;
    if (!next(c)) return false;
    return (c == '+') || (c == '-');
}

static bool intToken(TokenPtr &x) {
    char *begin = save();
    if (!sign()) restore(begin);
    char *p = save();
    if (hexInt()) goto success;
    restore(p);
    if (decimalInt()) goto success;
    return false;
success :
    char *end = save();
    x = new Token(T_INT_LITERAL, string(begin, end));
    return true;
}



//
// float tokens
//

static bool exponentPart() {
    char c;
    if (!next(c)) return false;
    if ((c != 'e') && (c != 'E')) return false;
    return decimalInt();
}

static bool fractionalPart() {
    char c;
    if (!next(c) || (c != '.')) return false;
    return decimalInt();
}

static bool floatToken(TokenPtr &x) {
    char *begin = save();
    if (!sign()) restore(begin);
    if (!decimalInt()) return false;
    char *p = save();
    if (fractionalPart()) {
        p = save();
        if (!exponentPart())
            restore(p);
    }
    else {
        restore(p);
        if (!exponentPart())
            return false;
    }
    char *end = save();
    x = new Token(T_FLOAT_LITERAL, string(begin, end));
    return true;
}



//
// suffix tokens
//

static bool suffixToken(TokenPtr &x) {
    char c;
    if (!next(c) || (c != '#')) return false;
    string s;
    if (!identStr(s)) return false;
    x = new Token(T_LITERAL_SUFFIX, s);
    return true;
}



//
// space
//

static bool isSpace(char c) {
    return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') ||
            (c == '\f') || (c == '\v'));
}

static bool space(TokenPtr &x) {
    char c;
    if (!next(c) || !isSpace(c)) return false;
    while (true) {
        char *p = save();
        if (!next(c) || !isSpace(c)) {
            restore(p);
            break;
        }
    }
    x = new Token(T_SPACE);
    return true;
}



//
// comments
//

static bool lineComment(TokenPtr &x) {
    char c;
    if (!next(c) || (c != '/')) return false;
    if (!next(c) || (c != '/')) return false;
    while (true) {
        char *p = save();
        if (!next(c)) break;
        if ((c == '\r') || (c == '\n')) {
            restore(p);
            break;
        }
    }
    x = new Token(T_LINE_COMMENT);
    return true;
}

static bool blockComment(TokenPtr &x) {
    char c;
    if (!next(c) || (c != '/')) return false;
    if (!next(c) || (c != '*')) return false;
    bool lastWasStar = false;
    while (true) {
        if (!next(c)) return false;
        if (lastWasStar && (c == '/'))
            break;
        lastWasStar = (c == '*');
    }
    x = new Token(T_BLOCK_COMMENT);
    return true;
}



//
// nextToken
//

static bool nextToken(TokenPtr &x) {
    char *p = save();
    if (space(x)) goto success;
    restore(p); if (lineComment(x)) goto success;
    restore(p); if (blockComment(x)) goto success;
    restore(p); if (symbol(x)) goto success;
    restore(p); if (keywordIdentifier(x)) goto success;
    restore(p); if (charToken(x)) goto success;
    restore(p); if (stringToken(x)) goto success;
    restore(p); if (floatToken(x)) goto success;
    restore(p); if (intToken(x)) goto success;
    restore(p); if (suffixToken(x)) goto success;
    if (p != end) {
        pushLocation(new Location(lexerSource, maxPtr-begin));
        error("invalid token");
    }
    return false;
success :
    assert(x);
    x->location = new Location(lexerSource, p-begin);
    return true;
}
