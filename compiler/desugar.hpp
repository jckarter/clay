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
StatementPtr desugarCatchBlocks(const vector<CatchPtr> &catchBlocks);
StatementPtr desugarSwitchStatement(SwitchPtr x);
BlockPtr desugarBlock(BlockPtr x);

ExprListPtr desugarEvalExpr(EvalExprPtr eval, EnvPtr env);
vector<StatementPtr> const &desugarEvalStatement(EvalStatementPtr eval, EnvPtr env);
vector<TopLevelItemPtr> const &desugarEvalTopLevel(EvalTopLevelPtr eval, EnvPtr env);

}

#endif // __DESUGAR_HPP
