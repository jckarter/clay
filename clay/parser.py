from clay.parserlib import *
from clay import tokens as t
from clay import lexer
from clay.ast import *
from clay.error import Location, raiseError

__all__ = ["parse"]

#
# primitives
#

def tokenType(klass) :
    def predicate(x) : return type(x) is klass
    return modify(condition(predicate), lambda x : x.value)

def tokenValue(klass, value) :
    def predicate(x) :
        return (type(x) is klass) and (x.value == value)
    return modify(condition(predicate), lambda x : x.value)

def symbol(s) : return tokenValue(t.Symbol, s)
def keyword(s) : return tokenValue(t.Keyword, s)

identTok = tokenType(t.Identifier)

semicolon = symbol(";")
comma = symbol(",")


#
# astNode
#

def astNode(parser, constructor) :
    def parserProc(input) :
        start_tok = input.pos
        result = parser(input)
        if result is Failure :
            return Failure
        node = constructor(result)
        node.location = input.data[start_tok].location
        return node
    return parserProc


#
# literals
#

boolLiteral = choice(astNode(keyword("true"), lambda x : BoolLiteral(True)),
                     astNode(keyword("false"), lambda x : BoolLiteral(False)))
intLiteral = astNode(tokenType(t.IntLiteral), lambda x : IntLiteral(x))
floatLiteral = astNode(tokenType(t.FloatLiteral), lambda x : FloatLiteral(x))
doubleLiteral = astNode(tokenType(t.DoubleLiteral),
                        lambda x : DoubleLiteral(x))
charLiteral = astNode(tokenType(t.CharLiteral), lambda x : CharLiteral(x))
stringLiteral = astNode(tokenType(t.StringLiteral),
                        lambda x : StringLiteral(x))
literal = choice(boolLiteral, intLiteral, floatLiteral, doubleLiteral,
                 charLiteral, stringLiteral)


#
# atomic expr
#

expression2 = lambda input : expression(input)
optExpression = optional(expression2)
expressionList = listOf(expression2, comma)
optExpressionList = modify(optional(expressionList),
                             lambda x : [] if x is None else x)

arrayExpr = astNode(sequence(symbol("["), expressionList, symbol("]")),
                    lambda x : ArrayExpr(x[1]))
tupleExpr = astNode(sequence(symbol("("), expressionList, symbol(")")),
                    lambda x : TupleExpr(x[1]))

identifier = astNode(identTok, lambda x : Identifier(x))
nameRef = astNode(identifier, lambda x : NameRef(x))
atomicExpr = choice(arrayExpr, tupleExpr, nameRef, literal)


#
# suffix expr
#

indexSuffix = astNode(sequence(symbol("["), expressionList, symbol("]")),
                      lambda x : IndexExpr(None,x[1]))
callSuffix = astNode(sequence(symbol("("), optExpressionList, symbol(")")),
                     lambda x : CallExpr(None,x[1]))
fieldRefSuffix = astNode(sequence(symbol("."), identifier),
                         lambda x : FieldRef(None,x[1]))
tupleRefSuffix = astNode(sequence(symbol("."), intLiteral),
                         lambda x : TupleRef(None,x[1]))
dereferenceSuffix = astNode(symbol("^"), lambda x : Dereference(None))

suffix = choice(indexSuffix, callSuffix, fieldRefSuffix,
                tupleRefSuffix, dereferenceSuffix)

def foldSuffixes(expr, suffixes) :
    for suffix in suffixes :
        suffix.expr = expr
        expr = suffix
    return expr

suffixExpr = modify(sequence(atomicExpr, zeroPlus(suffix)),
                    lambda x : foldSuffixes(x[0], x[1]))


#
# expression
#

addressOfExpr = astNode(sequence(symbol("&"), suffixExpr),
                        lambda x : AddressOfExpr(x[1]))

expression = choice(addressOfExpr, suffixExpr)


#
# typeSpec, variable
#

typeSpec = modify(sequence(symbol(":"), expression), lambda x : x[1])
optTypeSpec = optional(typeSpec)

variable = astNode(sequence(identifier, optTypeSpec),
                   lambda x : Variable(x[0],x[1]))

#
# statements
#

statement2 = lambda input : statement(input)

returnStatement = astNode(sequence(keyword("return"), optExpression,
                                   semicolon),
                          lambda x : ReturnStatement(x[1]))
parenCondition = modify(sequence(symbol("("), expression, symbol(")")),
                        lambda x : x[1])
elsePart = modify(sequence(keyword("else"), statement2),
                  lambda x : x[1])
ifStatement = astNode(sequence(keyword("if"), parenCondition,
                               statement2, optional(elsePart)),
                      lambda x : IfStatement(x[1],x[2],x[3]))

assignment = astNode(sequence(expression, symbol("="), expression, semicolon),
                     lambda x : Assignment(x[0], x[2]))
localVarDef = astNode(sequence(keyword("var"), variable, symbol("="),
                               expression, semicolon),
                      lambda x : LocalVarDef(x[1],x[3]))

block = astNode(sequence(symbol("{"), onePlus(statement2), symbol("}")),
                lambda x : Block(x[1]))

exprStatement = astNode(sequence(expression, semicolon),
                        lambda x : ExprStatement(x[0]))

statement = choice(block, localVarDef, assignment, ifStatement,
                   returnStatement, exprStatement)


#
# top level items
#

identifierList = listOf(identifier, comma)
typeVars = modify(sequence(symbol("["), identifierList, symbol("]")),
                  lambda x : x[1])
optTypeVars = modify(optional(typeVars), lambda x : [] if x is None else x)
fieldDef = astNode(sequence(identifier, typeSpec, semicolon),
                   lambda x : Field(x[0],x[1]))
recordDef = astNode(sequence(keyword("record"), identifier, optTypeVars,
                             symbol("{"), onePlus(fieldDef), symbol("}")),
                    lambda x : RecordDef(x[1],x[2],x[4]))
globalVarDef = astNode(sequence(keyword("var"), variable, symbol("="),
                                expression, semicolon),
                       lambda x : GlobalVarDef(x[1],x[3]))

isRef = modify(optional(keyword("ref")), lambda x : x is not None)

valueArgument = astNode(sequence(isRef, variable),
                        lambda x : ValueArgument(x[0],x[1]))
typeArgument = astNode(sequence(keyword("type"), expression),
                       lambda x : TypeArgument(x[1]))
argument = choice(valueArgument, typeArgument)
optArguments = modify(optional(listOf(argument, comma)),
                      lambda x : [] if x is None else x)
typeCondition = astNode(sequence(identifier, symbol("("), optExpressionList,
                                 symbol(")")),
                        lambda x : TypeCondition(x[0],x[2]))
typeConditions = modify(sequence(keyword("if"), listOf(typeCondition,comma)),
                        lambda x : x[1])
optTypeConditions = modify(optional(typeConditions),
                           lambda x : [] if x is None else x)
procedure = astNode(sequence(optTypeVars, symbol("("), optArguments,
                             symbol(")"), isRef, optTypeSpec,
                             optTypeConditions, block),
                    lambda x : Procedure(x[0],x[2],x[4],x[5],x[6],x[7]))
procedureDef = astNode(sequence(keyword("def"), identifier, procedure),
                       lambda x : ProcedureDef(x[1], x[2]))
overloadableDef = astNode(sequence(keyword("overloadable"), identifier,
                                   semicolon),
                          lambda x : OverloadableDef(x[1]))
overloadDef = astNode(sequence(keyword("overload"), identifier, procedure),
                      lambda x : OverloadDef(x[1], x[2]))

topLevelItem = choice(recordDef, globalVarDef, procedureDef,
                      overloadableDef, overloadDef)


#
# program
#

program = astNode(onePlus(topLevelItem), lambda x : Program(x))


#
# parse
#

def parse(data, fileName) :
    tokens = lexer.tokenize(data, fileName)
    input = Input(tokens)
    result = program(input)
    if (result is Failure) or (input.pos < len(tokens)) :
        if input.maxPos == len(tokens) :
            location = Location(data, len(data), fileName)
        else :
            location = tokens[input.maxPos].location
        raiseError("parse error", location)
    return result
