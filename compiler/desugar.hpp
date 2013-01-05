#ifndef __DESUGAR_HPP
#define __DESUGAR_HPP

#include "clay.hpp"

namespace clay {

ExprPtr desugarCharLiteral(char c);
void desugarFieldRef(FieldRefPtr x, ModulePtr module);
ExprPtr desugarStaticIndexing(StaticIndexingPtr x);
ExprPtr desugarVariadicOp(VariadicOpPtr x);
ExprListPtr desugarVariadicAssignmentRight(VariadicAssignment *x);
StatementPtr desugarForStatement(ForPtr x);
StatementPtr desugarCatchBlocks(llvm::ArrayRef<CatchPtr> catchBlocks);
StatementPtr desugarSwitchStatement(SwitchPtr x);

ExprListPtr desugarEvalExpr(EvalExprPtr eval, EnvPtr env);
llvm::ArrayRef<StatementPtr> desugarEvalStatement(EvalStatementPtr eval, EnvPtr env);
llvm::ArrayRef<TopLevelItemPtr> desugarEvalTopLevel(EvalTopLevelPtr eval, EnvPtr env);
OverloadPtr desugarAsOverload(OverloadPtr &x);

}

#endif // __DESUGAR_HPP
