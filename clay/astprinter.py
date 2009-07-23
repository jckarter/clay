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

r(PredicateDef, lambda x : xo("PredicateDef", x.name))
r(InstanceDef, lambda x : xo("InstanceDef", x.name, x.type_vars,
                             tuple(x.type_args), xf("if",x.type_conditions)))
r(TypeCondition, lambda x : xo("TypeCondition", x.name.s, *x.type_args))
r(RecordDef, lambda x : xo("RecordDef", x.name, x.type_vars, *x.fields))
r(StructDef, lambda x : xo("StructDef", x.name, x.type_vars, *x.fields))
r(Field, lambda x : xo("Field", x.name, x.type))
r(Variable, lambda x : xo("Variable", x.name, x.type))
r(VariableDef, lambda x : xo("VariableDef", x.variables, x.expr))
r(ProcedureDef, lambda x : xo("ProcedureDef", x.name, x.type_vars,
                              tuple(x.args), x.return_type,
                              xf("if",x.type_conditions), x.body))
r(OverloadableDef, lambda x : xo("OverloadableDef", x.name))
r(OverloadDef, lambda x : xo("OverloadDef", x.name, x.type_vars,
                             tuple(x.args), x.return_type,
                             xf("if",x.type_conditions), x.body))
r(ValueArgument, lambda x : xo("ValueArgument", xf("ref",x.is_ref),
                               x.variable))
r(TypeArgument, lambda x : xo("TypeArgument", x.type))

r(Name, lambda x : xo("Name", XSymbol(x.s)))

#
# statements
#

r(Block, lambda x : xo("Block", *x.statements))
r(LocalVariableDef, lambda x : xo("LocalVariableDef", x.variables, x.expr))
r(Assignment, lambda x : xo("Assignment", x.assignables, x.expr))
r(IfStatement, lambda x : xo("IfStatement", x.condition,
                             xf("then", x.then_part),
                             xf("else", x.else_part)))
r(BreakStatement, lambda x : xo("Break"))
r(ContinueStatement, lambda x : xo("Continue"))
r(WhileStatement, lambda x : xo("WhileStatement", x.condition, x.body))
r(ForStatement, lambda x : xo("ForStatement", x.variables, x.expr, x.body))
r(ReturnStatement, lambda x : xo("ReturnStatement", *x.results))


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
r(NameRef, lambda x : xo("NameRef", XSymbol(x.s)))
r(BoolLiteral, lambda x : xo("BoolLiteral", x.value))
r(IntLiteral, lambda x : xo("IntLiteral", x.value))
r(CharLiteral, lambda x : xo("CharLiteral", x.value))
r(StringLiteral, lambda x : xo("StringLiteral", x.value))
