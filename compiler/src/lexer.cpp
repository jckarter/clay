#include "clay.hpp"

namespace clay {

static void initLexer(SourcePtr s, int offset, int length);
static void cleanupLexer();
static bool nextToken(TokenPtr &x);

void tokenize(SourcePtr source, vector<TokenPtr> &tokens) {
    tokenize(source, 0, source->size, tokens);
}

void tokenize(SourcePtr source, int offset, int length,
              vector<TokenPtr> &tokens) {
    initLexer(source, offset, length);
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
static int beginOffset;
static char *begin;
static char *ptr;
static char *end;
static char *maxPtr;

static void initLexer(SourcePtr source, int offset, int length) {
    lexerSource = source.ptr();
    begin = source->data + offset;
    end = begin + length;
    ptr = begin;
    maxPtr = begin;
    beginOffset = offset;
}

static void cleanupLexer() {
    lexerSource = NULL;
    begin = ptr = end = maxPtr = NULL;
}

static LocationPtr locationFor(char *ptr) {
    ptrdiff_t offset = (ptr - begin) + beginOffset;
    return new Location(lexerSource, offset);
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
    if (x == '?') return true;
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
        {"public", "private", "import", "as",
         "record", "variant", "instance",
         "define", "overload", "external", "alias",
         "rvalue", "ref", "forward",
         "inline", "enum", "var",
         "and", "or", "not",
         "if", "else", "goto", "return", "while",
         "switch", "case", "break", "continue", "for", "in",
         "true", "false", "try", "catch", "throw",
         "finally", "onerror",
         "eval", "with","when",
         "__FILE__", "__LINE__", "__COLUMN__", "__ARG__", NULL};
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
    "..", "::", "^",  
    "(", ")", "[", "]", "{", "}",
    ":", ";", ",", ".", "#",
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
// operator strings
//


static const char *opchars[] = {
    "<", ">","+", "-", "*", "/","\\","%", "=", "~", "|", "!", "&", NULL
};

static bool opstring(string &x) {
    const char **s = opchars;
    char *p = save();
    char *q = p;
    char y;
    while (*s && next(y)) {
        s = opchars;
        while (*s) {
            if (y == **s) {
                q = save();            
                break;
            }
            ++s;
        }
    }
    restore(q);
    if (p == q) return false;        
    x = string(p,q);
    return true;
}

static bool op(TokenPtr &x) {
    string s;
    if(!opstring(s)) return false;
    x = new Token(T_OPSTRING, s);
    return true;
}


static bool opIdentifier(TokenPtr &x) {
    char c;
    if (!next(c)) return false;
    if (c != '(') return false;
    string op;
    if(!opstring(op)) return false;
    if (!next(c)) return false;
    if (c != ')') return false;
    x = new Token(T_IDENTIFIER, op);
    return true;
}



//
// hex and decimal digits
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

static bool escapeChar(char &x) {
    char c;
    if (!next(c)) return false;
    if (c != '\\') return false;
    if (!next(c)) return false;
    switch (c) {
    case 'n' : x = '\n'; return true;
    case 'r' : x = '\r'; return true;
    case 't' : x = '\t'; return true;
    case 'f' : x = '\f'; return true;
    case '\\' :
    case '\'' :
    case '"' :
    case '$' :
        x = c; return true;
    case '0' :
        x = '\0'; return true;
    case 'x' :
        return hexEscapeChar(x);
    }
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

static bool singleQuoteStringToken(TokenPtr &x) {
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

static bool stringToken(TokenPtr &x) {
    char *p = save();
    char c;
    if (!next(c) || (c != '"')) return false;
    if (!next(c) || (c != '"') || !next(c) || (c != '"')) {
        restore(p);
        return singleQuoteStringToken(x);
    }
    string s;
    while (true) {
        p = save();
        if (next(c) && (c == '"')
            && next(c) && (c == '"')
            && next(c) && (c == '"'))
        {
            char *q = save();
            if (!next(c) || c != '"') {
                restore(q);
                break;
            }
        }

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

static void optNumericSeparator() {
    char *p = save();
    char c;
    if (!next(c) || (c != '_'))
        restore(p);
}

static bool decimalDigits() {
    int x;
    while (true) {
        optNumericSeparator();
        char *p = save();
        if (!decimalDigit(x)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool hexDigits() {
    int x;
    while (true) {
        optNumericSeparator();
        char *p = save();
        if (!hexDigit(x)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool hexInt() {
    if (!str("0x")) return false;
    int x;
    if (!hexDigit(x)) return false;
    return hexDigits();
}

static bool decimalInt() {
    int x;
    if (!decimalDigit(x)) return false;
    while (true) {
        optNumericSeparator();
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
    char *begin = save();
    if (!sign()) restore(begin);
    return decimalInt();
}

static bool fractionalPart() {
    char c;
    if (!next(c) || (c != '.')) return false;
    return decimalDigits();
}

static bool hexExponentPart() {
    char c;
    if (!next(c)) return false;
    if ((c != 'p') && (c != 'P')) return false;
    char *begin = save();
    if (!sign()) restore(begin);
    return decimalInt();
}

static bool hexFractionalPart() {
    char c;
    if (!next(c) || (c != '.')) return false;
    return hexDigits();
}

static bool floatToken(TokenPtr &x) {
    char *begin = save();
    if (!sign()) restore(begin);
    char *afterSign = save();
    if (hexInt()) {
        char *p = save();
        if (hexFractionalPart()) {
            if (!hexExponentPart())
                return false;
        } else {
            restore(p);
            if (!hexExponentPart())
                return false;
        }
        char *end = save();
        x = new Token(T_FLOAT_LITERAL, string(begin, end));
        return true;
    } else {
        restore(afterSign);
        if (decimalInt()) {
            char *p = save();
            if (fractionalPart()) {
                p = save();
                if (!exponentPart())
                    restore(p);
            } else {
                restore(p);
                if (!exponentPart())
                    return false;
            }
            char *end = save();
            x = new Token(T_FLOAT_LITERAL, string(begin, end));
            return true;
        } else
            return false;
    }
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
// llvmToken
//
//  LLVMBody -> '__llvm__' ZeroPlus(Space) Braces
//
//  Braces -> '{' Body '}'
//
//  Body -> ZeroPlus(BodyItem)
//
//  BodyItem -> Comment
//            | Braces
//            | StringLiteral
//            | Not('}')
//
//  Comment -> ';' ZeroPlus(Not('\n')) '\n'
//
//  StringLiteral -> '"' ZeroPlus(StringChar) '"'
//
//  StringChar -> '\' AnyChar
//              | Not('"')
//

static bool llvmBraces();
static bool llvmBody();
static bool llvmBodyItem();
static bool llvmComment();
static bool llvmStringLiteral();
static bool llvmStringChar();

static bool llvmToken(TokenPtr &x) {
    const char *prefix = "__llvm__";
    while (*prefix) {
        char c;
        if (!next(c)) return false;
        if (c != *prefix) return false;
        ++prefix;
    }
    char *p = save();
    while (true) {
        char c;
        if (!next(c)) return false;
        if (!isSpace(c))
            break;
        p = save();
    }
    restore(p);
    char *begin = save();
    if (!llvmBraces()) return false;
    char *end = save();
    x = new Token(T_LLVM, string(begin, end));
    x->location = locationFor(begin);
    return true;
}

static bool llvmBraces() {
    char c;
    if (!next(c) || (c != '{')) return false;
    if (!llvmBody()) return false;
    if (!next(c) || (c != '}')) return false;
    return true;
}

static bool llvmBody() {
    while (true) {
        char *p = save();
        if (!llvmBodyItem()) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool llvmBodyItem() {
    char *p = save();
    if (llvmComment()) return true;
    if (restore(p), llvmBraces()) return true;
    if (restore(p), llvmStringLiteral()) return true;
    char c;
    if (restore(p), (next(c) && (c != '}'))) return true;
    return false;
}

static bool llvmComment() {
    char c;
    if (!next(c) || (c != ';')) return false;
    while (next(c) && (c != '\n')) {}
    return true;
}

static bool llvmStringLiteral() {
    char c;
    if (!next(c) || (c != '"')) return false;
    while (true) {
        char *p = save();
        if (!llvmStringChar()) {
            restore(p);
            break;
        }
    }
    if (!next(c) || (c != '"')) return false;
    return true;
}

static bool llvmStringChar() {
    char c;
    if (!next(c)) return false;
    if (c == '\\')
        return next(c);
    return c != '"';
}


//
// static index
//

static bool staticIndex(TokenPtr &x) {
    char c;
    if (!next(c)) return false;
    if (c != '.') return false;

    char *begin = save();
    if (hexInt()) {
        char *end = save();
        x = new Token(T_STATIC_INDEX, string(begin, end));
        return true;
    } else {
        restore(begin);
        if (decimalInt()) {
            char *end = save();
            x = new Token(T_STATIC_INDEX, string(begin, end));
            return true;
        } else
            return false;
    }
}


//
// nextToken
//

static bool nextToken(TokenPtr &x) {
    char *p = save();
    if (space(x)) goto success;
    restore(p); if (lineComment(x)) goto success;
    restore(p); if (blockComment(x)) goto success;
    restore(p); if (staticIndex(x)) goto success;
    restore(p); if (opIdentifier(x)) goto success;
    restore(p); if (symbol(x)) goto success;
    restore(p); if (op(x)) goto success;
    restore(p); if (llvmToken(x)) goto success;
    restore(p); if (keywordIdentifier(x)) goto success;
    restore(p); if (charToken(x)) goto success;
    restore(p); if (stringToken(x)) goto success;
    restore(p); if (floatToken(x)) goto success;
    restore(p); if (intToken(x)) goto success;
    if (p != end) {
        pushLocation(locationFor(maxPtr));
        error("invalid token");
    }
    return false;
success :
    assert(x.ptr());
    if (!x->location)
        x->location = locationFor(p);
    return true;
}

}
