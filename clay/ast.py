class ASTNode(object) :
    start_token = None
    end_token = None
    def set_location(self, start_token, end_token) :
        self.start_token = start_token
        self.end_token = end_token

class Program(ASTNode) :
    def __init__(self, top_level_items) :
        self.top_level_items = top_level_items

class TopLevelItem(ASTNode) :
    pass

class RecordDef(TopLevelItem) :
    def __init__(self, name, type_vars, fields) :
        self.name = name
        self.type_vars = type_vars
        self.fields = fields

class Field(ASTNode) :
    def __init__(self, name, type) :
        self.name = name
        self.type = type

class VariableDef(TopLevelItem) :
    def __init__(self, name, expr) :
        self.name = name
        self.expr = expr

class ProcedureDef(TopLevelItem) :
    def __init__(self, name, args, body) :
        self.name = name
        self.args = args
        self.body = body

class Statement(ASTNode) :
    pass

class Block(Statement) :
    def __init__(self, statements) :
        self.statements = statements

class LocalVariableDef(Statement) :
    def __init__(self, name, expr) :
        self.name = name
        self.expr = expr

class Assignment(Statement) :
    def __init__(self, name, expr) :
        self.name = name
        self.expr = expr

class IfStatement(Statement) :
    def __init__(self, condition, then_part, else_part) :
        self.condition = condition
        self.then_part = then_part
        self.else_part = else_part

class ReturnStatement(Statement) :
    def __init__(self, expr) :
        self.expr = expr

class Expression(Statement) :
    pass

class IndexExpr(Expression) :
    def __init__(self, expr, indexes) :
        self.expr = expr
        self.indexes = indexes

class CallExpr(Expression) :
    def __init__(self, expr, args) :
        self.expr = expr
        self.args = args

class FieldRef(Expression) :
    def __init__(self, expr, name) :
        self.expr = expr
        self.name = name

class ArrayExpr(Expression) :
    def __init__(self, elements) :
        self.elements = elements

class TupleExpr(Expression) :
    def __init__(self, elements) :
        self.elements = elements

class NameRef(Expression) :
    def __init__(self, name) :
        self.name = name

class BoolLiteral(Expression) :
    def __init__(self, value) :
        self.value = value

class IntLiteral(Expression) :
    def __init__(self, value) :
        self.value = value

class CharLiteral(Expression) :
    def __init__(self, value) :
        self.value = value

class StringLiteral(Expression) :
    def __init__(self, value) :
        self.value = value
