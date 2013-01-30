#ifndef __PARSER_HPP
#define __PARSER_HPP

#include "clay.hpp"
#include "lexer.hpp"

namespace clay { 

enum ParserFlags
{
    NoParserFlags = 0,
    ParserKeepDocumentation = 1
};

struct ReplItem {
    bool isExprSet;
    ExprPtr expr;
    vector<TopLevelItemPtr> toplevels;
    vector<ImportPtr> imports;
    vector<StatementPtr> stmts;
};

ModulePtr parse(llvm::StringRef moduleName, SourcePtr source, 
                CompilerState* cst, ParserFlags flags = NoParserFlags);
ExprPtr parseExpr(SourcePtr source, unsigned offset, size_t length,
                  CompilerState* cst);
ExprListPtr parseExprList(SourcePtr source, unsigned offset, size_t length,
                          CompilerState* cst);
void parseStatements(SourcePtr source, unsigned offset, size_t length,
    vector<StatementPtr> &statements, CompilerState* cst);
void parseTopLevelItems(SourcePtr source, unsigned offset, size_t length,
    vector<TopLevelItemPtr> &topLevels, Module *, CompilerState* cst);
ReplItem parseInteractive(SourcePtr source, unsigned offset, size_t length,
                          CompilerState* cst);

typedef vector<Token>(*AddTokensCallback)();

}

#endif // __PARSER_HPP
