#ifndef __DESUGAR_HPP
#define __DESUGAR_HPP

#include "clay.hpp"

namespace clay {

ExprPtr desugarCharLiteral(char c, CompilerState* cst);
void desugarFieldRef(FieldRefPtr x, ModulePtr module);
ExprPtr desugarStaticIndexing(StaticIndexingPtr x, CompilerState* cst);
ExprPtr desugarVariadicOp(VariadicOpPtr x, CompilerState* cst);
ExprListPtr desugarVariadicAssignmentRight(VariadicAssignment *x);
StatementPtr desugarForStatement(ForPtr x, CompilerState* cst);
StatementPtr desugarCatchBlocks(llvm::ArrayRef<CatchPtr> catchBlocks,
                                CompilerState* cst);
void desugarThrow(Throw* thro);
StatementPtr desugarSwitchStatement(SwitchPtr x, CompilerState* cst);

ExprListPtr desugarEvalExpr(EvalExprPtr eval, EnvPtr env, CompilerState* cst);
llvm::ArrayRef<StatementPtr> desugarEvalStatement(EvalStatementPtr eval, EnvPtr env,
                                                  CompilerState* cst);
llvm::ArrayRef<TopLevelItemPtr> desugarEvalTopLevel(EvalTopLevelPtr eval, EnvPtr env);
OverloadPtr desugarAsOverload(OverloadPtr &x, CompilerState* cst);

}

#endif // __DESUGAR_HPP
