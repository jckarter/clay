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

stringLiteral = astNode(tokenType(t.StringLiteral),
                        lambda x : StringLiteral(x))
charLiteral = astNode(tokenType(t.CharLiteral), lambda x : CharLiteral(x))
intLiteral = astNode(tokenType(t.IntLiteral), lambda x : IntLiteral(x))
boolLiteral = choice(astNode(keyword("true"), lambda x : BoolLiteral(True)),
                     astNode(keyword("false"), lambda x : BoolLiteral(False)))
literal = choice(boolLiteral, intLiteral, charLiteral, stringLiteral)


#
# atomic expr
#

expression2 = lambda input : expression(input)
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
pointerRefSuffix = astNode(symbol("^"), lambda x : PointerRef(None))

suffix = choice(indexSuffix, callSuffix, fieldRefSuffix,
                tupleRefSuffix, pointerRefSuffix)

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
# typeSpec, variable, variableList
#

typeSpec = modify(sequence(symbol(":"), expression), lambda x : x[1])
optTypeSpec = optional(typeSpec)

variable = astNode(sequence(identifier, optTypeSpec),
                   lambda x : Variable(x[0],x[1]))
variableList = listOf(variable, comma)

#
# statements
#

statement2 = lambda input : statement(input)

returnStatement = astNode(sequence(keyword("return"), optExpressionList,
                                   semicolon),
                          lambda x : ReturnStatement(x[1]))
forStatement = astNode(sequence(keyword("for"), symbol("("), keyword("var"),
                                variableList, keyword("in"), expression,
                                symbol(")"), statement2),
                       lambda x : ForStatement(x[3], x[5], x[7]))
parenCondition = modify(sequence(symbol("("), expression, symbol(")")),
                        lambda x : x[1])
whileStatement = astNode(sequence(keyword("while"), parenCondition,
                                  statement2),
                         lambda x : WhileStatement(x[1],x[2]))
continueStatement = astNode(sequence(keyword("continue"), semicolon),
                            lambda x : ContinueStatement())
breakStatement = astNode(sequence(keyword("break"), semicolon),
                         lambda x : BreakStatement())
elsePart = modify(sequence(keyword("else"), statement2),
                  lambda x : x[1])
ifStatement = astNode(sequence(keyword("if"), parenCondition,
                               statement2, optional(elsePart)),
                      lambda x : IfStatement(x[1],x[2],x[3]))

assignment = astNode(sequence(expressionList, symbol("="), expressionList,
                              semicolon),
                     lambda x : Assignment(x[0], x[2]))
localVariableDef = astNode(sequence(keyword("var"), variableList,
                                    symbol("="), expressionList, semicolon),
                           lambda x : LocalVariableDef(x[1],x[3]))

block = astNode(sequence(symbol("{"), onePlus(statement2), symbol("}")),
                lambda x : Block(x[1]))

exprStatement = astNode(sequence(expression, semicolon),
                        lambda x : ExprStatement(x[0]))

statement = choice(block, localVariableDef, assignment,
                   ifStatement, breakStatement, continueStatement,
                   whileStatement, forStatement, returnStatement,
                   exprStatement)


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
structDef = astNode(sequence(keyword("struct"), identifier, optTypeVars,
                             symbol("{"), onePlus(fieldDef), symbol("}")),
                    lambda x : StructDef(x[1],x[2],x[4]))
variableDef = astNode(sequence(keyword("var"), variableList,
                               symbol("="), expressionList, semicolon),
                      lambda x : VariableDef(x[1],x[3]))

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
procedureDef = astNode(sequence(keyword("def"), identifier, optTypeVars,
                                symbol("("), optArguments, symbol(")"),
                                optTypeSpec, optTypeConditions, block),
                       lambda x : ProcedureDef(x[1],x[2],x[4],x[6],x[7],x[8]))

overloadableDef = astNode(sequence(keyword("overloadable"), identifier,
                                   semicolon),
                          lambda x : OverloadableDef(x[1]))

overloadDef = astNode(sequence(keyword("overload"), identifier, optTypeVars,
                               symbol("("), optArguments, symbol(")"),
                               optTypeSpec, optTypeConditions, block),
                      lambda x : OverloadDef(x[1],x[2],x[4],x[6],x[7],x[8]))

predicateDef = astNode(sequence(keyword("predicate"), identifier, semicolon),
                       lambda x : PredicateDef(x[1]))

instanceDef = astNode(sequence(keyword("instance"), identifier, optTypeVars,
                               symbol("("), optExpressionList, symbol(")"),
                               optTypeConditions, semicolon),
                      lambda x : InstanceDef(x[1],x[2],x[4],x[6]))

topLevelItem = choice(predicateDef, instanceDef, recordDef, structDef,
                      variableDef, procedureDef, overloadableDef,
                      overloadDef)


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
