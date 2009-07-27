__all__ = [
    "ASTNode", "Program", "TopLevelItem", "PredicateDef", "Identifier",
    "InstanceDef", "TypeCondition", "RecordDef", "StructDef", "Field",
    "Variable", "VariableDef", "ProcedureDef", "OverloadableDef",
    "OverloadDef", "Argument", "ValueArgument", "TypeArgument",
    "Statement", "Block", "LocalVariableDef", "Assignment",
    "IfStatement", "BreakStatement", "ContinueStatement",
    "WhileStatement", "ForStatement", "ReturnStatement", "ExprStatement",
    "Expression", "AddressOfExpr", "IndexExpr", "CallExpr",
    "FieldRef", "TupleRef", "PointerRef",
    "ArrayExpr", "TupleExpr", "NameRef", "BoolLiteral", "IntLiteral",
    "CharLiteral", "StringLiteral"]

class ASTNode(object) :
    location = None

def _check(x, t) :
    assert isinstance(x,t)

def _check2(x, t) :
    assert (x is None) or isinstance(x,t)

def _checklist(x, t) :
    _check(x, list)
    for item in x :
        _check(item, t)

class Program(ASTNode) :
    def __init__(self, top_level_items) :
        _checklist(top_level_items, TopLevelItem)
        self.top_level_items = top_level_items

class TopLevelItem(ASTNode) :
    pass

class PredicateDef(TopLevelItem) :
    def __init__(self, name) :
        _check(name, Identifier)
        self.name = name

class Identifier(ASTNode) :
    def __init__(self, s) :
        _check(s, str)
        self.s = s

class InstanceDef(TopLevelItem) :
    def __init__(self, name, type_vars, type_args, type_conditions) :
        _check(name, Identifier)
        _checklist(type_vars, Identifier)
        _checklist(type_args, Expression)
        _checklist(type_conditions, TypeCondition)
        self.name = name
        self.type_vars = type_vars
        self.type_args = type_args
        self.type_conditions = type_conditions

class TypeCondition(ASTNode) :
    def __init__(self, name, type_args) :
        _check(name, Identifier)
        _checklist(type_args, Expression)
        self.name = name
        self.type_args = type_args

class RecordDef(TopLevelItem) :
    def __init__(self, name, type_vars, fields) :
        _check(name, Identifier)
        _checklist(type_vars, Identifier)
        _checklist(fields, Field)
        self.name = name
        self.type_vars = type_vars
        self.fields = fields

class StructDef(TopLevelItem) :
    def __init__(self, name, type_vars, fields) :
        _check(name, Identifier)
        _checklist(type_vars, Identifier)
        _checklist(fields, Field)
        self.name = name
        self.type_vars = type_vars
        self.fields = fields

class Field(ASTNode) :
    def __init__(self, name, type) :
        _check(name, Identifier)
        _check(type, Expression)
        self.name = name
        self.type = type

class Variable(ASTNode) :
    def __init__(self, name, type) :
        _check(name, Identifier)
        _check2(type, Expression)
        self.name = name
        self.type = type

class VariableDef(TopLevelItem) :
    def __init__(self, variables, exprlist) :
        _checklist(variables, Variable)
        _checklist(exprlist, Expression)
        self.variables = variables
        self.exprlist = exprlist

class ProcedureDef(TopLevelItem) :
    def __init__(self, name, type_vars, args, return_type,
                 type_conditions, body) :
        _check(name, Identifier)
        _checklist(type_vars, Identifier)
        _checklist(args, Argument)
        _check2(return_type, Expression)
        _checklist(type_conditions, TypeCondition)
        _check(body, Block)
        self.name = name
        self.type_vars = type_vars
        self.args = args
        self.return_type = return_type
        self.type_conditions = type_conditions
        self.body = body

class OverloadableDef(TopLevelItem) :
    def __init__(self, name) :
        _check(name, Identifier)
        self.name = name

class OverloadDef(TopLevelItem) :
    def __init__(self, name, type_vars, args, return_type,
                 type_conditions, body) :
        _check(name, Identifier)
        _checklist(type_vars, Identifier)
        _checklist(args, Argument)
        _check2(return_type, Expression)
        _checklist(type_conditions, TypeCondition)
        _check(body, Block)
        self.name = name
        self.type_vars = type_vars
        self.args = args
        self.return_type = return_type
        self.type_conditions = type_conditions
        self.body = body

class Argument(ASTNode) :
    pass

class ValueArgument(Argument) :
    def __init__(self, is_ref, variable) :
        _check(is_ref, bool)
        _check(variable, Variable)
        self.is_ref = is_ref
        self.variable = variable

class TypeArgument(Argument) :
    def __init__(self, type) :
        _check(type, Expression)
        self.type = type

class Statement(ASTNode) :
    pass

class Block(Statement) :
    def __init__(self, statements) :
        _checklist(statements, Statement)
        self.statements = statements

class LocalVariableDef(Statement) :
    def __init__(self, variables, exprlist) :
        _checklist(variables, Variable)
        _checklist(exprlist, Expression)
        self.variables = variables
        self.exprlist = exprlist

class Assignment(Statement) :
    def __init__(self, assignables, exprlist) :
        _checklist(assignables, Expression)
        _checklist(exprlist, Expression)
        self.assignables = assignables
        self.exprlist = exprlist

class IfStatement(Statement) :
    def __init__(self, condition, then_part, else_part) :
        _check(condition, Expression)
        _check(then_part, Statement)
        _check2(else_part, Statement)
        self.condition = condition
        self.then_part = then_part
        self.else_part = else_part

class BreakStatement(Statement) :
    pass

class ContinueStatement(Statement) :
    pass

class WhileStatement(Statement) :
    def __init__(self, condition, body) :
        _check(condition, Expression)
        _check(body, Statement)
        self.condition = condition
        self.body = body

class ForStatement(Statement) :
    def __init__(self, variables, expr, body) :
        _checklist(variables, Variable)
        _check(expr, Expression)
        _check(body, Statement)
        self.variables = variables
        self.expr = expr
        self.body = body

class ReturnStatement(Statement) :
    def __init__(self, exprlist) :
        _checklist(exprlist, Expression)
        self.exprlist = exprlist

class ExprStatement(Statement) :
    def __init__(self, expr) :
        _check(expr, Expression)
        self.expr = expr

class Expression(ASTNode) :
    pass

class AddressOfExpr(Expression) :
    def __init__(self, expr) :
        _check(expr, Expression)
        self.expr = expr

class IndexExpr(Expression) :
    def __init__(self, expr, indexes) :
        _check2(expr, Expression)
        _checklist(indexes, Expression)
        self.expr = expr
        self.indexes = indexes

class CallExpr(Expression) :
    def __init__(self, expr, args) :
        _check2(expr, Expression)
        _checklist(args, Expression)
        self.expr = expr
        self.args = args

class FieldRef(Expression) :
    def __init__(self, expr, name) :
        _check2(expr, Expression)
        _check(name, Identifier)
        self.expr = expr
        self.name = name

class TupleRef(Expression) :
    def __init__(self, expr, index) :
        _check2(expr, Expression)
        _check(index, int)
        self.expr = expr
        self.index = index

class PointerRef(Expression) :
    def __init__(self, expr) :
        _check2(expr, Expression)
        self.expr = expr

class ArrayExpr(Expression) :
    def __init__(self, elements) :
        _checklist(elements, Expression)
        self.elements = elements

class TupleExpr(Expression) :
    def __init__(self, elements) :
        _checklist(elements, Expression)
        self.elements = elements

class NameRef(Expression) :
    def __init__(self, name) :
        _check(name, Identifier)
        self.name = name

class BoolLiteral(Expression) :
    def __init__(self, value) :
        _check(value, bool)
        self.value = value

class IntLiteral(Expression) :
    def __init__(self, value) :
        assert isinstance(value,int) or isinstance(value,long)
        self.value = value

class CharLiteral(Expression) :
    def __init__(self, value) :
        _check(value, unicode)
        self.value = value

class StringLiteral(Expression) :
    def __init__(self, value) :
        _check(value, unicode)
        self.value = value
