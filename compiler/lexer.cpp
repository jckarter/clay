#include "clay.hpp"
#include "lexer.hpp"


namespace clay {

static void initLexer(SourcePtr s, unsigned offset, size_t length);
static void cleanupLexer();
static bool nextToken(Token &x);
static bool nextDocToken(Token &x);

void tokenize(SourcePtr source, vector<Token> &tokens) {
    tokenize(source, 0, source->size(), tokens);
}

void tokenize(SourcePtr source, unsigned offset, size_t length,
              vector<Token> &tokens) {
    initLexer(source, offset, length);
    tokens.push_back(Token());
    while (nextToken(tokens.back())) {
        switch (tokens.back().tokenKind) {
        case T_SPACE :
        case T_LINE_COMMENT :
        case T_BLOCK_COMMENT :
            break;
        case T_DOC_START:
            while (nextDocToken(tokens.back())) {
                if (tokens.back().tokenKind == T_DOC_END)
                    break;
                tokens.push_back(Token());
            }
            break;
        default :
            tokens.push_back(Token());
        }
    }
    tokens.pop_back();
    cleanupLexer();
}

static Source *lexerSource;
static unsigned beginOffset;
static const char *begin;
static const char *ptr;
static const char *end;
static const char *maxPtr;

static void initLexer(SourcePtr source, unsigned offset, size_t length) {
    lexerSource = source.ptr();
    begin = source->data() + offset;
    end = begin + length;
    ptr = begin;
    maxPtr = begin;
    beginOffset = offset;
}

static void cleanupLexer() {
    lexerSource = NULL;
    begin = ptr = end = maxPtr = NULL;
}

static Location locationFor(const char *ptr) {
    unsigned offset = unsigned(ptr - begin) + beginOffset;
    return Location(lexerSource, offset);
}

static const char *save() { return ptr; }
static void restore(const char *p) { ptr = p; }

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


static bool identStr(llvm::SmallString<16> &x) {
    char c;
    if (!identChar1(c)) return false;
    x.clear();
    x.push_back(c);
    while (true) {
        const char *p = save();
        if (!identChar2(c)) {
            restore(p);
            break;
        }
        x.push_back(c);
    }
    return true;
}

static std::set<llvm::StringRef> *keywords = NULL;

static void initKeywords() {
    const char *s[] =
        {"public", "private", "import", "as",
         "record", "variant", "instance",
         "define", "overload", "default",
         "external", "alias",
         "rvalue", "ref", "forward",
         "inline", "noinline", "forceinline",
         "enum", "var", "and", "or", "not",
         "if", "else", "goto", "return", "while",
         "switch", "case", "break", "continue", "for", "in",
         "true", "false", "try", "catch", "throw",
         "finally", "onerror", "staticassert",
         "eval", "when", "newtype",
         "__FILE__", "__LINE__", "__COLUMN__", "__ARG__", NULL};
    keywords = new std::set<llvm::StringRef>();
    for (const char **p = s; *p; ++p)
        keywords->insert(*p);
}

static bool keywordIdentifier(Token &x) {
    x.str.clear();
    if (!identStr(x.str)) return false;
    if (!keywords) initKeywords();
    if (keywords->find(x.str) != keywords->end())
        x.tokenKind = T_KEYWORD;
    else
        x.tokenKind = T_IDENTIFIER;
    return true;
}



//
// symbols
//

static const char *symbols[] = {
    "..", "::", "^", "@",  
    "(", ")", "[", "]", "{", "}",
    ":", ";", ",", ".", "#",
    NULL
};

static bool symbol(Token &x) {
    const char **s = symbols;
    const char *p = save();
    while (*s) {
        restore(p);
        if (str(*s)) {
            x = Token(T_SYMBOL, *s);
            return true;
        }
        ++s;
    }
    return false;
}

//
// operator strings
//


static llvm::StringRef opchars("=!<>+-*/\\%~|&");

static bool opstring(llvm::SmallString<16> &x) {
    const char *p = save();
    const char *q = p;
    char y;
    while (next(y)) {
        if (opchars.find(y) != llvm::StringRef::npos)
            q = save();
        else
            break;
    }
    restore(q);
    if (p == q) return false;
    x = llvm::StringRef(p, (size_t)(q-p));
    return true;
}

static bool op(Token &x) {
    x.str.clear();
    if(!opstring(x.str)) return false;
    char c;
    const char *p = save();
    if(next(c) && c == ':') {
        x.tokenKind = T_UOPSTRING;
    } else {
        restore(p);
        x.tokenKind = T_OPSTRING;
    }
    return true;
}

static bool opIdentifier(Token &x) {
    char c;
    if (!next(c)) return false;
    if (c != '(') return false;
    x.str.clear();
    if(!opstring(x.str)) return false;
    if (!next(c)) return false;
    if (c != ')') return false;
    x.tokenKind = T_IDENTIFIER;
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
    const char *p = save();
    if (escapeChar(x)) return true;
    restore(p);
    if (!next(x)) return false;
    if (x == '\\') return false;
    return true;
}

static bool charToken(Token &x) {
    char c;
    if (!next(c) || (c != '\'')) return false;
    const char *p = save();
    if (next(c) && (c == '\'')) return false;
    restore(p);
    char v;
    if (!oneChar(v)) return false;
    if (!next(c) || (c != '\'')) return false;
    x = Token(T_CHAR_LITERAL, llvm::StringRef(&v,1));
    return true;
}

static bool singleQuoteStringToken(Token &x) {
    char c;
    if (!next(c) || (c != '"')) return false;
    x.tokenKind = T_STRING_LITERAL;
    x.str.clear();
    while (true) {
        const char *p = save();
        if (next(c) && (c == '"'))
            break;
        restore(p);
        if (!oneChar(c)) return false;
        x.str.push_back(c);
    }
    return true;
}

static bool stringToken(Token &x) {
    const char *p = save();
    char c;
    if (!next(c) || (c != '"')) return false;
    if (!next(c) || (c != '"') || !next(c) || (c != '"')) {
        restore(p);
        return singleQuoteStringToken(x);
    }
    x.tokenKind = T_STRING_LITERAL;
    x.str.clear();
    while (true) {
        p = save();
        if (next(c) && (c == '"')
            && next(c) && (c == '"')
            && next(c) && (c == '"'))
        {
            const char *q = save();
            if (!next(c) || c != '"') {
                restore(q);
                break;
            }
        }

        restore(p);
        if (!oneChar(c)) return false;
        x.str.push_back(c);
    }
    return true;
}



//
// integer tokens
//

static void optNumericSeparator() {
    const char *p = save();
    char c;
    if (!next(c) || (c != '_'))
        restore(p);
}

static bool decimalDigits() {
    int x;
    while (true) {
        optNumericSeparator();
        const char *p = save();
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
        const char *p = save();
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
        const char *p = save();
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

static bool intToken(Token &x) {
    const char *begin = save();
    if (!sign()) restore(begin);
    const char *p = save();
    if (hexInt()) goto success;
    restore(p);
    if (decimalInt()) goto success;
    return false;
success :
    const char *end = save();
    x = Token(T_INT_LITERAL, llvm::StringRef(begin, (size_t)(end-begin)));
    return true;
}



//
// float tokens
//

static bool exponentPart() {
    char c;
    if (!next(c)) return false;
    if ((c != 'e') && (c != 'E')) return false;
    const char *begin = save();
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
    const char *begin = save();
    if (!sign()) restore(begin);
    return decimalInt();
}

static bool hexFractionalPart() {
    char c;
    if (!next(c) || (c != '.')) return false;
    return hexDigits();
}

static bool floatToken(Token &x) {
    const char *begin = save();
    if (!sign()) restore(begin);
    const char *afterSign = save();
    if (hexInt()) {
        const char *p = save();
        if (hexFractionalPart()) {
            if (!hexExponentPart())
                return false;
        } else {
            restore(p);
            if (!hexExponentPart())
                return false;
        }
        const char *end = save();
        x = Token(T_FLOAT_LITERAL, llvm::StringRef(begin, (size_t)(end-begin)));
        return true;
    } else {
        restore(afterSign);
        if (decimalInt()) {
            const char *p = save();
            if (fractionalPart()) {
                p = save();
                if (!exponentPart())
                    restore(p);
            } else {
                restore(p);
                if (!exponentPart())
                    return false;
            }
            const char *end = save();
            x = Token(T_FLOAT_LITERAL, llvm::StringRef(begin, (size_t)(end-begin)));
            return true;
        } else
            return false;
    }
}



//
// space
//

bool isSpace(char c) {
    return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') ||
            (c == '\f') || (c == '\v'));
}

static bool space(Token &x) {
    char c;
    if (!next(c) || !isSpace(c)) return false;
    while (true) {
        const char *p = save();
        if (!next(c) || !isSpace(c)) {
            restore(p);
            break;
        }
    }
    x = Token(T_SPACE);
    return true;
}



//
// comments
//

static bool lineComment(Token &x) {
    char c;
    if (!next(c) || (c != '/')) return false;
    if (!next(c) || (c != '/')) return false;
    while (true) {
        const char *p = save();
        if (!next(c)) break;
        if ((c == '\r') || (c == '\n')) {
            restore(p);
            break;
        }
    }
    x = Token(T_LINE_COMMENT);
    return true;
}

static bool blockComment(Token &x) {
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
    x = Token(T_BLOCK_COMMENT);
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

static bool llvmToken(Token &x) {
    const char *prefix = "__llvm__";
    while (*prefix) {
        char c;
        if (!next(c)) return false;
        if (c != *prefix) return false;
        ++prefix;
    }
    const char *p = save();
    while (true) {
        char c;
        if (!next(c)) return false;
        if (!isSpace(c))
            break;
        p = save();
    }
    restore(p);
    const char *begin = save();
    if (!llvmBraces()) return false;
    const char *end = save();
    x = Token(T_LLVM, llvm::StringRef(begin, (size_t)(end-begin)));
    x.location = locationFor(begin);
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
        const char *p = save();
        if (!llvmBodyItem()) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool llvmBodyItem() {
    const char *p = save();
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
        const char *p = save();
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

static bool staticIndex(Token &x) {
    char c;
    if (!next(c)) return false;
    if (c != '.') return false;

    const char *begin = save();
    if (hexInt()) {
        const char *end = save();
        x = Token(T_STATIC_INDEX, llvm::StringRef(begin, (size_t)(end-begin)));
        return true;
    } else {
        restore(begin);
        if (decimalInt()) {
            const char *end = save();
            x = Token(T_STATIC_INDEX, llvm::StringRef(begin, (size_t)(end-begin)));
            return true;
        } else
            return false;
    }
}


//
// nextToken
//

static bool docStartLine(Token &x);
static bool docStartBlock(Token &x);

static bool nextToken(Token &x) {
    x = Token();
    const char *p = save();
    if (space(x)) goto success;
    restore(p); if (docStartLine(x)) goto success;
    restore(p); if (docStartBlock(x)) goto success;
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
        pushLocation(locationFor(p));
        error("invalid token");
    }
    return false;
success :
    assert(x.tokenKind != T_NONE);
    if (!x.location.ok())
        x.location = locationFor(p);
    return true;
}

//
// documentation
//


static bool docIsBlock = false;
static bool docStartLine(Token &x) {
    char c;
    if (!next(c) || (c != '/')) return false;
    if (!next(c) || (c != '/')) return false;
    if (!next(c) || (c != '/')) return false;
    x = Token(T_DOC_START);
    docIsBlock = false;
    return true;
}

static bool docStartBlock(Token &x) {
    char c;
    if (!next(c) || (c != '/')) return false;
    if (!next(c) || (c != '*')) return false;
    if (!next(c) || (c != '*')) return false;

    docIsBlock = true;
    x = Token(T_DOC_START);
    return true;
}


static bool docEndLine(Token &x) {
    char c;
    if (!next(c)) return false;

    if ((c == '\r') || (c == '\n')) {
        x = Token(T_DOC_END);
        return true;
    }
    return false;
}

static bool docEndBlock(Token &x) {
    char c;
    do {
        if (!next(c)) return false;
    } while (c == '*');
    if (c != '/') return false;
    x = Token(T_DOC_END);
    return true;
}

static bool docProperty(Token &x) {
    char c;
    if (!next(c)) return false;
    while (isSpace(c))
        if (!next(c))
            break;
    if (c != '@') return false;


    const char *begin = save();
    while (!isSpace(c))
        if (!next(c)) return false;

    const char *end = save();
    x = Token(T_DOC_PROPERTY, llvm::StringRef(begin, (size_t)(end - begin - 1)));
    x.location = locationFor(begin);
    return true;
}

static bool maybeDocEnd()
{
    char c = '*';
    while (c == '*')
        if (!next(c))
            return false;
    return (c == '/');
}

static bool docText(Token &x) {

    const char *begin = save();
    char c;
    if (!next(c)) return false;

    const char *end = begin;
    for (;;) {
        end = save();
        if (!next(c)) return false;
        if (c == '\n' || c == '\r') {
            restore(end);
            break;
        }
        end = save();
        if (docIsBlock && (c == '*')) {
            if (maybeDocEnd()) {
                restore(end);
                break;
            } else {
                restore(end);
            }
        }
    }
    x = Token(T_DOC_TEXT, llvm::StringRef(begin, (size_t)(end - begin)));
    return true;
}

static bool docSpace()
{
    char c;
    if (!next(c)) return false;
    if (!docIsBlock && (c == '\n' || c == '\r'))
        return false;
    else if (isSpace(c))
        return true;
    else if (docIsBlock && c == '*') {
        const char *p = save();
        if (!next(c))
            return true;
        if (c == '/') {
            return false;
        }
        restore(p);
        return true;
    }
    return false;
}

static bool nextDocToken(Token &x) {
    x = Token();
    const char *p = save();

    for (;;) {
        p = save();
        if(!docSpace()) {
            restore(p);
            break;
        }
    }

    if (docIsBlock) {
        if (docEndBlock(x))  goto success;
    } else {
        if (docEndLine(x))  goto success;
    }

    restore(p); if (docProperty(x))  goto success;
    restore(p); if (docText(x)) goto success;
    if (p != end) {
        pushLocation(locationFor(p));
        error("invalid doc token");
    }
    return false;
success :
    assert(x.tokenKind != T_NONE);
    if (!x.location.ok())
        x.location = locationFor(p);
    return true;
}

}
