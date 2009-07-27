from clay.ast import *
from clay.xprint import xprint, XObject, XField, XSymbol, xregister

__all__ = ["ast_print"]

def ast_print(x, columns=80) :
    xprint(x, columns)


r = xregister
xo = XObject
xf = XField


#
# program
#

r(Program, lambda x : xo("Program", *x.topLevelItems))


#
# top level items
#

r(PredicateDef, lambda x : xo("PredicateDef", x.name))
r(InstanceDef, lambda x : xo("InstanceDef", x.name, x.typeVars,
                             tuple(x.typeArgs), xf("if",x.typeConditions)))
r(TypeCondition, lambda x : xo("TypeCondition", x.name.s, *x.typeArgs))
r(RecordDef, lambda x : xo("RecordDef", x.name, x.typeVars, *x.fields))
r(StructDef, lambda x : xo("StructDef", x.name, x.typeVars, *x.fields))
r(Field, lambda x : xo("Field", x.name, x.type))
r(Variable, lambda x : xo("Variable", x.name, x.type))
r(VariableDef, lambda x : xo("VariableDef", x.variables, x.exprList))
r(ProcedureDef, lambda x : xo("ProcedureDef", x.name, x.typeVars,
                              tuple(x.args), x.returnType,
                              xf("if",x.typeConditions), x.body))
r(OverloadableDef, lambda x : xo("OverloadableDef", x.name))
r(OverloadDef, lambda x : xo("OverloadDef", x.name, x.typeVars,
                             tuple(x.args), x.returnType,
                             xf("if",x.typeConditions), x.body))
r(ValueArgument, lambda x : xo("ValueArgument", xf("ref",x.isRef),
                               x.variable))
r(TypeArgument, lambda x : xo("TypeArgument", x.type))

r(Identifier, lambda x : xo("Identifier", XSymbol(x.s)))

#
# statements
#

r(Block, lambda x : xo("Block", *x.statements))
r(LocalVariableDef, lambda x : xo("LocalVariableDef", x.variables, x.exprList))
r(Assignment, lambda x : xo("Assignment", x.assignables, x.exprList))
r(IfStatement, lambda x : xo("IfStatement", x.condition,
                             xf("then", x.thenPart),
                             xf("else", x.elsePart)))
r(BreakStatement, lambda x : xo("Break"))
r(ContinueStatement, lambda x : xo("Continue"))
r(WhileStatement, lambda x : xo("WhileStatement", x.condition, x.body))
r(ForStatement, lambda x : xo("ForStatement", x.variables, x.expr, x.body))
r(ReturnStatement, lambda x : xo("ReturnStatement", *x.exprList))
r(ExprStatement, lambda x : xo("ExprStatement", x.expr))


#
# expressions
#

r(AddressOfExpr, lambda x : xo("AddressOfExpr", x.expr))

r(IndexExpr, lambda x : xo("IndexExpr", x.expr, xf("indexes",x.indexes)))
r(CallExpr, lambda x : xo("CallExpr", x.expr, xf("args",x.args)))
r(FieldRef, lambda x : xo("FieldRef", x.expr, xf("field",x.name)))
r(TupleRef, lambda x : xo("TupleRef", x.expr, xf("index",x.index)))
r(PointerRef, lambda x : xo("PointerRef", x.expr))

r(ArrayExpr, lambda x : xo("ArrayExpr", *x.elements))
r(TupleExpr, lambda x : xo("TupleExpr", *x.elements))
r(NameRef, lambda x : xo("NameRef", x.name))
r(BoolLiteral, lambda x : xo("BoolLiteral", x.value))
r(IntLiteral, lambda x : xo("IntLiteral", x.value))
r(CharLiteral, lambda x : xo("CharLiteral", x.value))
r(StringLiteral, lambda x : xo("StringLiteral", x.value))
