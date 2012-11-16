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

ModulePtr parse(llvm::StringRef moduleName, SourcePtr source, ParserFlags flags = NoParserFlags);
ExprPtr parseExpr(SourcePtr source, int offset, int length);
ExprListPtr parseExprList(SourcePtr source, int offset, int length);
void parseStatements(SourcePtr source, int offset, int length,
    vector<StatementPtr> &statements);
void parseTopLevelItems(SourcePtr source, int offset, int length,
    vector<TopLevelItemPtr> &topLevels, Module *);
void parseInteractive(SourcePtr source, int offset, int length,
                      vector<TopLevelItemPtr>& toplevels,
                      vector<ImportPtr>& imports,
                      vector<StatementPtr>& stmts);

typedef vector<Token>(*AddTokensCallback)();
void setAddTokens(AddTokensCallback f);

}

#endif // __PARSER_HPP
