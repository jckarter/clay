#pragma once


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

ModulePtr parse(llvm::StringRef moduleName, SourcePtr source, ParserFlags flags = NoParserFlags);
ExprPtr parseExpr(SourcePtr source, unsigned offset, size_t length);
ExprListPtr parseExprList(SourcePtr source, unsigned offset, size_t length);
void parseStatements(SourcePtr source, unsigned offset, size_t length,
    vector<StatementPtr> &statements);
void parseTopLevelItems(SourcePtr source, unsigned offset, size_t length,
    vector<TopLevelItemPtr> &topLevels, Module *);
ReplItem parseInteractive(SourcePtr source, unsigned offset, size_t length);

typedef vector<Token>(*AddTokensCallback)();
void setAddTokens(AddTokensCallback f);

}
