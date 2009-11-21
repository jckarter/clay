__all__ = [
    "ASTNode", "Identifier", "DottedName",
    "Expression", "BoolLiteral", "IntLiteral", "FloatLiteral",
    "CharLiteral", "StringLiteral",
    "NameRef", "Tuple", "Array", "Indexing", "Call", "FieldRef", "TupleRef",
    "Dereference", "AddressOf",
    "UnaryOpExpr", "BinaryOpExpr", "NotExpr", "AndExpr", "OrExpr",
    "StaticExpr",
    "Statement", "Block", "Label",
    "VarBinding", "RefBinding", "StaticBinding",
    "Assignment", "Goto", "Return", "IfStatement", "ExprStatement",
    "While", "Break", "Continue", "For",
    "Code", "FormalArgument", "ValueArgument", "StaticArgument",
    "TopLevelItem",
    "Record", "RecordArg", "ValueRecordArg", "StaticRecordArg",
    "Procedure", "Overloadable", "Overload",
    "ExternalProcedure", "ExternalArg",
    "Import", "Export", "Module"]

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

class DottedName(ASTNode) :
    def __init__(self, names) :
        super(DottedName, self).__init__()
        checkList(names, Identifier)
        self.names = names



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
    def __init__(self, value, suffix) :
        super(IntLiteral, self).__init__()
        assert isinstance(value, int) or isinstance(value, long)
        check2(suffix, str)
        self.value = value
        self.suffix = suffix

class FloatLiteral(Expression) :
    def __init__(self, value, suffix) :
        super(FloatLiteral, self).__init__()
        assert isinstance(value, float)
        check2(suffix, str)
        self.value = value
        self.suffix = suffix

class CharLiteral(Expression) :
    def __init__(self, value) :
        super(CharLiteral, self).__init__()
        check(value, str)
        self.value = value

class StringLiteral(Expression) :
    def __init__(self, value) :
        super(StringLiteral, self).__init__()
        check(value, str)
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

class Array(Expression) :
    def __init__(self, args) :
        super(Array, self).__init__()
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

# operators: + -
class UnaryOpExpr(Expression) :
    def __init__(self, op, expr) :
        super(UnaryOpExpr, self).__init__()
        check(op, str)
        check(expr, Expression)
        self.op = op
        self.expr = expr

# operators: + - * / == != < <= > >=
class BinaryOpExpr(Expression) :
    def __init__(self, op, expr1, expr2) :
        super(BinaryOpExpr, self).__init__()
        check(op, str)
        check2(expr1, Expression)
        check(expr2, Expression)
        self.op = op
        self.expr1 = expr1
        self.expr2 = expr2

class NotExpr(Expression) :
    def __init__(self, expr) :
        super(NotExpr, self).__init__()
        check(expr, Expression)
        self.expr = expr

class AndExpr(Expression) :
    def __init__(self, expr1, expr2) :
        super(AndExpr, self).__init__()
        check2(expr1, Expression)
        check(expr2, Expression)
        self.expr1 = expr1
        self.expr2 = expr2

class OrExpr(Expression) :
    def __init__(self, expr1, expr2) :
        super(OrExpr, self).__init__()
        check2(expr1, Expression)
        check(expr2, Expression)
        self.expr1 = expr1
        self.expr2 = expr2

class StaticExpr(Expression) :
    def __init__(self, expr) :
        super(StaticExpr, self).__init__()
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

class Label(Statement) :
    def __init__(self, name) :
        super(Label, self).__init__()
        check(name, Identifier)
        self.name = name

class VarBinding(Statement) :
    def __init__(self, name, expr) :
        super(VarBinding, self).__init__()
        check(name, Identifier)
        check(expr, Expression)
        self.name = name
        self.expr = expr

class RefBinding(Statement) :
    def __init__(self, name, expr) :
        super(RefBinding, self).__init__()
        check(name, Identifier)
        check(expr, Expression)
        self.name = name
        self.expr = expr

class StaticBinding(Statement) :
    def __init__(self, name, expr) :
        super(StaticBinding, self).__init__()
        check(name, Identifier)
        check(expr, Expression)
        self.name = name
        self.expr = expr

class Assignment(Statement) :
    def __init__(self, left, right) :
        super(Assignment, self).__init__()
        check(left, Expression)
        check(right, Expression)
        self.left = left
        self.right = right

class Goto(Statement) :
    def __init__(self, labelName) :
        super(Goto, self).__init__()
        check(labelName, Identifier)
        self.labelName = labelName

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

class While(Statement) :
    def __init__(self, condition, body) :
        super(While, self).__init__()
        check(condition, Expression)
        check(body, Statement)
        self.condition = condition
        self.body = body

class Break(Statement) :
    def __init__(self) :
        super(Break, self).__init__()

class Continue(Statement) :
    def __init__(self) :
        super(Continue, self).__init__()

class For(Statement) :
    def __init__(self, variable, expr, body) :
        super(For, self).__init__()
        check(variable, Identifier)
        check(expr, Expression)
        check(body, Statement)
        self.variable = variable
        self.expr = expr
        self.body = body



#
# Code
#

class Code(ASTNode) :
    def __init__(self, typeVars, predicate, formalArgs,
                 returnByRef, returnType, body) :
        super(Code, self).__init__()
        checkList(typeVars, Identifier)
        check2(predicate, Expression)
        checkList(formalArgs, FormalArgument)
        check(returnByRef, bool)
        check2(returnType, Expression)
        check(body, Block)
        self.typeVars = typeVars
        self.predicate = predicate
        self.formalArgs = formalArgs
        self.returnByRef = returnByRef
        self.returnType = returnType
        self.body = body
        self.env = None

class FormalArgument(ASTNode) :
    pass

class ValueArgument(FormalArgument) :
    def __init__(self, name, type) :
        super(ValueArgument, self).__init__()
        check(name, Identifier)
        check2(type, Expression)
        self.name = name
        self.type = type

class StaticArgument(FormalArgument) :
    def __init__(self, pattern) :
        super(StaticArgument, self).__init__()
        check(pattern, Expression)
        self.pattern = pattern



#
# TopLevelItem
#

class TopLevelItem(ASTNode) :
    pass

class Record(TopLevelItem) :
    def __init__(self, name, typeVars, args) :
        super(Record, self).__init__()
        check(name, Identifier)
        checkList(typeVars, Identifier)
        checkList(args, RecordArg)
        self.name = name
        self.typeVars = typeVars
        self.args = args
        self.env = None
        self.valueArgs_ = None
        self.valueNames_ = None

class RecordArg(ASTNode) :
    pass

class ValueRecordArg(RecordArg) :
    def __init__(self, name, type) :
        super(ValueRecordArg, self).__init__()
        check(name, Identifier)
        check(type, Expression)
        self.name = name
        self.type = type

class StaticRecordArg(RecordArg) :
    def __init__(self, pattern) :
        super(StaticRecordArg, self).__init__()
        check(pattern, Expression)
        self.pattern = pattern

class Procedure(TopLevelItem) :
    def __init__(self, name, code) :
        super(Procedure, self).__init__()
        check(name, Identifier)
        check(code, Code)
        self.name = name
        self.code = code

class Overloadable(TopLevelItem) :
    def __init__(self, name) :
        super(Overloadable, self).__init__()
        check(name, Identifier)
        self.name = name
        self.overloads = []

class Overload(TopLevelItem) :
    def __init__(self, name, code) :
        super(Overload, self).__init__()
        check(name, Identifier)
        check(code, Code)
        self.name = name
        self.code = code

class ExternalProcedure(TopLevelItem) :
    def __init__(self, name, args, returnType) :
        super(ExternalProcedure, self).__init__()
        check(name, Identifier)
        checkList(args, ExternalArg)
        check(returnType, Expression)
        self.name = name
        self.args = args
        self.returnType = returnType
        self.env = None
        self.llvmFunc = None

class ExternalArg(ASTNode) :
    def __init__(self, name, type_) :
        super(ExternalArg, self).__init__()
        check(name, Identifier)
        check(type_, Expression)
        self.name = name
        self.type = type_



#
# Import, Export
#

class Import(ASTNode) :
    def __init__(self, dottedName) :
        super(Import, self).__init__()
        check(dottedName, DottedName)
        self.dottedName = dottedName
        self.module = None

class Export(ASTNode) :
    def __init__(self, dottedName) :
        super(Export, self).__init__()
        check(dottedName, DottedName)
        self.dottedName = dottedName
        self.module = None



#
# Module
#

class Module(ASTNode) :
    def __init__(self, imports, exports, topLevelItems) :
        super(Module, self).__init__()
        checkList(imports, Import)
        checkList(exports, Export)
        checkList(topLevelItems, TopLevelItem)
        self.imports = imports
        self.exports = exports
        self.topLevelItems = topLevelItems
        self.globals = {}
        self.env = None
        self.initialized1 = False
        self.initialized2 = False



#
# enable xprint support for ast
#

import clay.astprinter
