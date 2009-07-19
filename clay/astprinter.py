from clay.ast import *
from clay.xprint import xprint, XObject, XField, XSymbol, xregister

__all__ = ["ast_print"]

def ast_print(x) :
    xprint(x)


r = xregister
xo = XObject
xf = XField


#
# program
#

r(Program, lambda x : xo("Program", *x.top_level_items))


#
# top level items
#

r(RecordDef, lambda x : xo("RecordDef", x.name, x.type_vars, *x.fields))
r(Field, lambda x : xf(x.name,x.type,separator=":"))
r(VariableDef, lambda x : xo("VariableDef", x.name, x.type, x.expr))
r(ProcedureDef, lambda x : xo("ProcedureDef", x.name, x.type_vars,
                              tuple(x.args), x.body))
r(Argument, lambda x : (x.name, x.type))


#
# statements
#

r(Block, lambda x : xo("Block", *x.statements))
r(LocalVariableDef, lambda x : xo("LocalVariableDef", x.name, x.type, x.expr))
r(Assignment, lambda x : xo("Assignment", x.name, x.expr))
r(IfStatement, lambda x : xo("IfStatement", x.condition,
                             xf("then", x.then_part),
                             xf("else", x.else_part)))
r(ReturnStatement, lambda x : xo("ReturnStatement", x.expr))


#
# expressions
#

r(IndexExpr, lambda x : xo("IndexExpr", x.expr, xf("indexes",x.indexes)))
r(CallExpr, lambda x : xo("CallExpr", x.expr, xf("args",x.args)))
r(FieldRef, lambda x : xo("FieldRef", x.expr, xf("field",x.name)))
r(ArrayExpr, lambda x : xo("ArrayExpr", *x.elements))
r(TupleExpr, lambda x : xo("TupleExpr", *x.elements))
r(NameRef, lambda x : xo("NameRef", x.name))
r(BoolLiteral, lambda x : xo("BoolLiteral", x.value))
r(IntLiteral, lambda x : xo("IntLiteral", x.value))
r(CharLiteral, lambda x : xo("CharLiteral", x.value))
r(StringLiteral, lambda x : xo("StringLiteral", x.value))
