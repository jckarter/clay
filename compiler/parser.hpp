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

ModulePtr parse(llvm::StringRef moduleName, SourcePtr source, ParserFlags flags = NoParserFlags);
ExprPtr parseExpr(SourcePtr source, unsigned int offset, unsigned int length);
ExprListPtr parseExprList(SourcePtr source, unsigned int offset, unsigned int length);
void parseStatements(SourcePtr source, unsigned int offset, unsigned int length,
    vector<StatementPtr> &statements);
void parseTopLevelItems(SourcePtr source, unsigned int offset, unsigned int length,
    vector<TopLevelItemPtr> &topLevels, Module *);
ReplItem parseInteractive(SourcePtr source, unsigned int offset, unsigned int length);

typedef vector<Token>(*AddTokensCallback)();
void setAddTokens(AddTokensCallback f);

}

#endif // __PARSER_HPP
