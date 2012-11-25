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
    vector<TopLevelItemPtr> toplevels;
    vector<ImportPtr> imports;
    vector<StatementPtr> stmts;
};

ModulePtr parse(llvm::StringRef moduleName, SourcePtr source, ParserFlags flags = NoParserFlags);
ExprPtr parseExpr(SourcePtr source, int offset, int length);
ExprListPtr parseExprList(SourcePtr source, int offset, int length);
void parseStatements(SourcePtr source, int offset, int length,
    vector<StatementPtr> &statements);
void parseTopLevelItems(SourcePtr source, int offset, int length,
    vector<TopLevelItemPtr> &topLevels, Module *);
ReplItem parseInteractive(SourcePtr source, int offset, int length);

typedef vector<Token>(*AddTokensCallback)();
void setAddTokens(AddTokensCallback f);

}

#endif // __PARSER_HPP
