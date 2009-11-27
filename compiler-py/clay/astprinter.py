from clay.ast import *
from clay.xprint import xprint, XObject, XField, XSymbol, xregister

__all__ = []


r = xregister
xo = XObject
xf = XField


r(Identifier, lambda x : xo("Identifier", XSymbol(x.s)))
r(DottedName, lambda x : xo("DottedName", *x.names))



#
# Expression
#

r(BoolLiteral, lambda x : xo("BoolLiteral", x.value))
r(IntLiteral, lambda x : xo("IntLiteral", x.value, x.suffix))
r(FloatLiteral, lambda x : xo("FloatLiteral", x.value, x.suffix))
r(CharLiteral, lambda x : xo("CharLiteral", x.value))
r(StringLiteral, lambda x : xo("StringLiteral", x.value))

r(NameRef, lambda x : xo("NameRef", x.name))
r(Tuple, lambda x : xo("Tuple", *x.args))
r(Array, lambda x : xo("Array", *x.args))
r(Indexing, lambda x : xo("Indexing", x.expr, xf("args",x.args)))
r(Call, lambda x : xo("Call", x.expr, xf("args",x.args)))
r(FieldRef, lambda x : xo("FieldRef", x.expr, x.name))
r(TupleRef, lambda x : xo("TupleRef", x.expr, x.index))
r(Dereference, lambda x : xo("Dereference", x.expr))
r(AddressOf, lambda x : xo("AddressOf", x.expr))

r(UnaryOpExpr, lambda x : xo("UnaryOpExpr", x.op, x.expr))
r(BinaryOpExpr, lambda x : xo("BinaryOpExpr", x.op, x.expr1, x.expr2))

r(NotExpr, lambda x : xo("NotExpr", x.expr))
r(AndExpr, lambda x : xo("AndExpr", x.expr1, x.expr2))
r(OrExpr, lambda x : xo("OrExpr", x.expr1, x.expr2))



#
# Statement
#

r(Block, lambda x : xo("Block", *x.statements))
r(Label, lambda x : xo("Label", x.name))
r(VarBinding, lambda x : xo("VarBinding", x.name, x.expr))
r(RefBinding, lambda x : xo("RefBinding", x.name, x.expr))
r(StaticBinding, lambda x : xo("StaticBinding", x.name, x.expr))
r(Assignment, lambda x : xo("Assignment", x.left, x.right))
r(Goto, lambda x : xo("Goto", x.labelName))
r(Return, lambda x : xo("Return", x.expr))
r(ReturnRef, lambda x : xo("ReturnRef", x.expr))
r(IfStatement, lambda x : xo("IfStatement", x.condition,
                             xf("then", x.thenPart),
                             xf("else", x.elsePart)))
r(ExprStatement, lambda x : xo("ExprStatement", x.expr))
r(While, lambda x : xo("While", x.condition, x.body))
r(Break, lambda x : xo("Break"))
r(Continue, lambda x : xo("Continue"))
r(For, lambda x : xo("For", x.variable, x.expr, x.body))



#
# Code
#

r(Code, lambda x : xo("Code", x.patternVars, xf("when",x.predicate),
                      tuple(x.formalArgs), x.body))
r(ValueArgument, lambda x : xo("ValueArgument", x.name, x.type))
r(StaticArgument, lambda x : xo("StaticArgument", x.pattern))



#
# TopLevelItem
#

r(Record, lambda x : xo("Record", x.name, x.patternVars, *x.args))
r(ValueRecordArg, lambda x : xo("ValueRecordArg", x.name, x.type))
r(StaticRecordArg, lambda x : xo("StaticRecordArg", x.pattern))
r(Procedure, lambda x : xo("Procedure", x.name, x.code))
r(Overloadable, lambda x : xo("Overloadable", x.name))
r(Overload, lambda x : xo("Overload", x.name, x.code))
r(ExternalProcedure, lambda x : xo("ExternalProcedure", x.name, x.args,
                                   x.returnType))
r(ExternalArg, lambda x : xo("ExternalArg", x.name, x.type))



#
# Import, Export
#

r(Import, lambda x : xo("Import", x.dottedName))
r(Export, lambda x : xo("Export", x.dottedName))



#
# Module
#

r(Module, lambda x : xo("Module", *(x.imports + x.exports
                                    + x.topLevelItems)))
