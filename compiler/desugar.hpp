#ifndef __DESUGAR_HPP
#define __DESUGAR_HPP

#include "clay.hpp"

namespace clay {

ExprPtr desugarCharLiteral(char c, CompilerStatePtr cst);
void desugarFieldRef(FieldRefPtr x, ModulePtr module);
ExprPtr desugarStaticIndexing(StaticIndexingPtr x, CompilerStatePtr cst);
ExprPtr desugarVariadicOp(VariadicOpPtr x, CompilerStatePtr cst);
ExprListPtr desugarVariadicAssignmentRight(VariadicAssignment *x);
StatementPtr desugarForStatement(ForPtr x, CompilerStatePtr cst);
StatementPtr desugarCatchBlocks(llvm::ArrayRef<CatchPtr> catchBlocks,
                                CompilerStatePtr cst);
void desugarThrow(Throw* thro);
StatementPtr desugarSwitchStatement(SwitchPtr x, CompilerStatePtr cst);

ExprListPtr desugarEvalExpr(EvalExprPtr eval, EnvPtr env, CompilerStatePtr cst);
llvm::ArrayRef<StatementPtr> desugarEvalStatement(EvalStatementPtr eval, EnvPtr env,
                                                  CompilerStatePtr cst);
llvm::ArrayRef<TopLevelItemPtr> desugarEvalTopLevel(EvalTopLevelPtr eval, EnvPtr env);
OverloadPtr desugarAsOverload(OverloadPtr &x, CompilerStatePtr cst);

}

#endif // __DESUGAR_HPP
