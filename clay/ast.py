__all__ = [
    "ASTNode", "Identifier",
    "Expression", "BoolLiteral", "IntLiteral", "FloatLiteral",
    "DoubleLiteral", "CharLiteral", "StringLiteral",
    "NameRef", "Tuple", "Indexing", "Call", "FieldRef", "TupleRef",
    "Dereference", "AddressOf",
    "Statement", "Block", "Assignment", "LocalBinding", "Return",
    "IfStatement", "ExprStatement",
    "Code", "FormalArgument", "ValueArgument", "StaticArgument",
    "TopLevelItem", "Record", "Field", "Procedure", "Overloadable", "Overload",
    "Program"]

def check(x, t) :
    assert isinstance(x,t)

def check2(x, t) :
    assert (x is None) or isinstance(x,t)

def checkList(x, t) :
    check(x, list)
    for item in x :
        check(item, t)

class ASTNode(object) :
    location = None

class Identifier(ASTNode) :
    def __init__(self, s) :
        super(Identifier, self).__init__()
        check(s, str)
        self.s = s



#
# Expression
#

class Expression(ASTNode) :
    pass

class BoolLiteral(Expression) :
    def __init__(self, value) :
        super(BoolLiteral, self).__init__()
        check(value, bool)
        self.value = value

class IntLiteral(Expression) :
    def __init__(self, value) :
        super(IntLiteral, self).__init__()
        assert isinstance(value, int) or isinstance(value, long)
        self.value = value

class FloatLiteral(Expression) :
    def __init__(self, value) :
        super(FloatLiteral, self).__init__()
        check(value, str)
        self.value = value

class DoubleLiteral(Expression) :
    def __init__(self, value) :
        super(DoubleLiteral, self).__init__()
        check(value, str)
        self.value = value

class CharLiteral(Expression) :
    def __init__(self, value) :
        super(CharLiteral, self).__init__()
        check(value, unicode)
        self.value = value

class StringLiteral(Expression) :
    def __init__(self, value) :
        super(StringLiteral, self).__init__()
        check(value, unicode)
        self.value = value

class NameRef(Expression) :
    def __init__(self, name) :
        super(NameRef, self).__init__()
        check(name, Identifier)
        self.name = name

class Tuple(Expression) :
    def __init__(self, args) :
        super(Tuple, self).__init__()
        checkList(args, Expression)
        self.args = args

class Indexing(Expression) :
    def __init__(self, expr, args) :
        super(Indexing, self).__init__()
        check2(expr, Expression)
        checkList(args, Expression)
        self.expr = expr
        self.args = args

class Call(Expression) :
    def __init__(self, expr, args) :
        super(Call, self).__init__()
        check2(expr, Expression)
        checkList(args, Expression)
        self.expr = expr
        self.args = args

class FieldRef(Expression) :
    def __init__(self, expr, name) :
        super(FieldRef, self).__init__()
        check2(expr, Expression)
        check(name, Identifier)
        self.expr = expr
        self.name = name

class TupleRef(Expression) :
    def __init__(self, expr, index) :
        super(TupleRef, self).__init__()
        check2(expr, Expression)
        check(index, int)
        self.expr = expr
        self.index = index

class Dereference(Expression) :
    def __init__(self, expr) :
        super(Dereference, self).__init__()
        check2(expr, Expression)
        self.expr = expr

class AddressOf(Expression) :
    def __init__(self, expr) :
        super(AddressOf, self).__init__()
        check(expr, Expression)
        self.expr = expr



#
# Statement
#

class Statement(ASTNode) :
    pass

class Block(Statement) :
    def __init__(self, statements) :
        super(Block, self).__init__()
        checkList(statements, Statement)
        self.statements = statements

class Assignment(Statement) :
    def __init__(self, assignable, expr) :
        super(Assignment, self).__init__()
        check(assignable, Expression)
        check(expr, Expression)
        self.assignable = assignable
        self.expr = expr

class LocalBinding(Statement) :
    def __init__(self, name, type, expr) :
        super(LocalBinding, self).__init__()
        check(name, Identifier)
        check2(type, Expression)
        check(expr, Expression)
        self.name = name
        self.type = type
        self.expr = expr

class Return(Statement) :
    def __init__(self, expr) :
        super(Return, self).__init__()
        check2(expr, Expression)
        self.expr = expr

class IfStatement(Statement) :
    def __init__(self, condition, thenPart, elsePart) :
        super(IfStatement, self).__init__()
        check(condition, Expression)
        check(thenPart, Statement)
        check2(elsePart, Statement)
        self.condition = condition
        self.thenPart = thenPart
        self.elsePart = elsePart

class ExprStatement(Statement) :
    def __init__(self, expr) :
        super(ExprStatement, self).__init__()
        check(expr, Expression)
        self.expr = expr



#
# Code
#

class Code(ASTNode) :
    def __init__(self, typeVars, formalArgs, returnByRef,
                 returnType, typeConditions, body) :
        super(Code, self).__init__()
        checkList(typeVars, Identifier)
        checkList(formalArgs, FormalArgument)
        check(returnByRef, bool)
        check2(returnType, Expression)
        checkList(typeConditions, Expression)
        check(body, Block)
        self.typeVars = typeVars
        self.formalArgs = formalArgs
        self.returnByRef = returnByRef
        self.returnType = returnType
        self.typeConditions = typeConditions
        self.body = body

class FormalArgument(ASTNode) :
    pass

class ValueArgument(FormalArgument) :
    def __init__(self, name, type, byRef) :
        super(ValueArgument, self).__init__()
        check(name, Identifier)
        check2(type, Expression)
        check(byRef, bool)
        self.name = name
        self.type = type
        self.byRef = byRef

class StaticArgument(FormalArgument) :
    def __init__(self, type) :
        super(StaticArgument, self).__init__()
        check(type, Expression)
        self.type = type



#
# TopLevelItem
#

class TopLevelItem(ASTNode) :
    pass

class Record(TopLevelItem) :
    def __init__(self, name, typeVars, fields) :
        super(Record, self).__init__()
        check(name, Identifier)
        checkList(typeVars, Identifier)
        checkList(fields, Field)
        self.name = name
        self.typeVars = typeVars
        self.fields = fields
        self.env = None

class Field(ASTNode) :
    def __init__(self, name, type) :
        super(Field, self).__init__()
        check(name, Identifier)
        check(type, Expression)
        self.name = name
        self.type = type

class Procedure(TopLevelItem) :
    def __init__(self, name, code) :
        super(Procedure, self).__init__()
        check(name, Identifier)
        check(code, Code)
        self.name = name
        self.code = code
        self.env = None

class Overloadable(TopLevelItem) :
    def __init__(self, name) :
        super(Overloadable, self).__init__()
        check(name, Identifier)
        self.name = name
        self.env = None
        self.overloads = []

class Overload(TopLevelItem) :
    def __init__(self, name, code) :
        super(Overload, self).__init__()
        check(name, Identifier)
        check(code, Code)
        self.name = name
        self.code = code
        self.env = None



#
# Program
#

class Program(ASTNode) :
    def __init__(self, topLevelItems) :
        super(Program, self).__init__()
        checkList(topLevelItems, TopLevelItem)
        self.topLevelItems = topLevelItems



#
# enable xprint support for ast
#

import clay.astprinter
