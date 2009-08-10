from clay.ast import *
from clay.xprint import xprint, XObject, XField, XSymbol, xregister

__all__ = []


r = xregister
xo = XObject
xf = XField


r(Identifier, lambda x : xo("Identifier", XSymbol(x.s)))



#
# Expression
#

r(BoolLiteral, lambda x : xo("BoolLiteral", x.value))
r(IntLiteral, lambda x : xo("IntLiteral", x.value))
r(FloatLiteral, lambda x : xo("FloatLiteral", x.value))
r(DoubleLiteral, lambda x : xo("DoubleLiteral", x.value))
r(CharLiteral, lambda x : xo("CharLiteral", x.value))
r(StringLiteral, lambda x : xo("StringLiteral", x.value))

r(NameRef, lambda x : xo("NameRef", x.name))
r(Tuple, lambda x : xo("Tuple", *x.args))
r(Indexing, lambda x : xo("Indexing", x.expr, xf("args",x.args)))
r(Call, lambda x : xo("Call", x.expr, xf("args",x.args)))
r(FieldRef, lambda x : xo("FieldRef", x.expr, x.name))
r(TupleRef, lambda x : xo("TupleRef", x.expr, x.index))
r(Dereference, lambda x : xo("Dereference", x.expr))
r(AddressOf, lambda x : xo("AddressOf", x.expr))



#
# Statement
#

r(Block, lambda x : xo("Block", *x.statements))
r(Assignment, lambda x : xo("Assignment", x.assignable, x.expr))
r(LocalBinding, lambda x : xo("LocalBinding", x.name, x.type, x.expr))
r(Return, lambda x : xo("Return", x.expr))
r(IfStatement, lambda x : xo("IfStatement", x.condition,
                             xf("then", x.thenPart),
                             xf("else", x.elsePart)))
r(ExprStatement, lambda x : xo("ExprStatement", x.expr))



#
# Code
#

r(Code, lambda x : xo("Code", x.typeVars, tuple(x.formalArgs), x.returnByRef,
                      x.returnType, xf("if",x.typeConditions), x.body))
r(ValueArgument, lambda x : xo("ValueArgument", x.name, x.type,
                               xf("byRef",x.byRef)))
r(TypeArgument, lambda x : xo("TypeArgument", x.type))



#
# TopLevelItem
#

r(Record, lambda x : xo("RecordDef", x.name, x.typeVars, *x.fields))
r(Field, lambda x : xo("Field", x.name, x.type))
r(Procedure, lambda x : xo("Procedure", x.name, x.code))
r(Overloadable, lambda x : xo("Overloadable", x.name))
r(Overload, lambda x : xo("Overload", x.name, x.code))



#
# Program
#

r(Program, lambda x : xo("Program", *x.topLevelItems))
