from clay.parserlib import *
from clay import tokens as t
from clay import lexer
from clay.ast import *
from clay.error import Location, error

__all__ = ["parse"]

#
# combinators
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
# misc
#

semicolon = symbol(";")
comma = symbol(",")

identifier = astNode(tokenType(t.Identifier), lambda x : Identifier(x))

dottedName = astNode(listOf(identifier, comma), lambda x : DottedName(x))


#
# literals
#

boolLiteral = choice(astNode(keyword("true"), lambda x : BoolLiteral(True)),
                     astNode(keyword("false"), lambda x : BoolLiteral(False)))
intLiteral = astNode(sequence(tokenType(t.IntLiteral),
                              optional(tokenType(t.LiteralSuffix))),
                     lambda x : IntLiteral(x[0], x[1]))
floatLiteral = astNode(sequence(tokenType(t.FloatLiteral),
                                optional(tokenType(t.LiteralSuffix))),
                       lambda x :FloatLiteral(x[0], x[1]))
charLiteral = astNode(tokenType(t.CharLiteral), lambda x : CharLiteral(x))
stringLiteral = astNode(tokenType(t.StringLiteral),
                        lambda x : StringLiteral(x))
literal = choice(boolLiteral, intLiteral, floatLiteral,
                 charLiteral, stringLiteral)


#
# misc
#

expression2 = lambda input : expression(input)
optExpression = optional(expression2)
expressionList = listOf(expression2, comma)
optExpressionList = modify(optional(expressionList),
                             lambda x : [] if x is None else x)


#
# atomic expr
#

arrayExpr = astNode(sequence(symbol("["), expressionList, symbol("]")),
                    lambda x : Array(x[1]))
tupleExpr = astNode(sequence(symbol("("), expressionList, symbol(")")),
                    lambda x : Tuple(x[1]))
nameRef = astNode(identifier, lambda x : NameRef(x))
atomicExpr = choice(arrayExpr, tupleExpr, nameRef, literal)


#
# suffix expr
#

indexingSuffix = astNode(sequence(symbol("["), expressionList, symbol("]")),
                         lambda x : Indexing(None,x[1]))
callSuffix = astNode(sequence(symbol("("), optExpressionList, symbol(")")),
                     lambda x : Call(None,x[1]))
fieldRefSuffix = astNode(sequence(symbol("."), identifier),
                         lambda x : FieldRef(None,x[1]))
tupleRefSuffix = astNode(sequence(symbol("."), intLiteral),
                         lambda x : TupleRef(None,x[1]))
dereferenceSuffix = astNode(symbol("^"), lambda x : Dereference(None))

suffix = choice(indexingSuffix, callSuffix, fieldRefSuffix,
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
                        lambda x : AddressOf(x[1]))

plusOrMinus = choice(symbol("+"), symbol("-"))

signExpr = astNode(sequence(plusOrMinus, suffixExpr),
                   lambda x : UnaryOpExpr(x[0], x[1]))

prefixExpr = choice(signExpr, addressOfExpr, suffixExpr)

mulDivOp = choice(symbol("*"), symbol("/"), symbol("%"))
mulDivTail = astNode(sequence(mulDivOp, prefixExpr),
                     lambda x : BinaryOpExpr(x[0], None, x[1]))
mulDivExpr = modify(sequence(prefixExpr, zeroPlus(mulDivTail)),
                    lambda x : foldBinaryOps(x[0], x[1]))

addSubTail = astNode(sequence(plusOrMinus, mulDivExpr),
                     lambda x : BinaryOpExpr(x[0], None, x[1]))
addSubExpr = modify(sequence(mulDivExpr, zeroPlus(addSubTail)),
                    lambda x : foldBinaryOps(x[0], x[1]))

compareOp = choice(symbol("=="), symbol("!="),
                   symbol("<"), symbol("<="),
                   symbol(">"), symbol(">="))
compareTail = astNode(sequence(compareOp, addSubExpr),
                      lambda x : BinaryOpExpr(x[0], None, x[1]))
compareExpr = modify(sequence(addSubExpr, optional(compareTail)),
                     lambda x : (x[0] if x[1] is None
                                 else foldBinaryOps(x[0], [x[1]])))

def foldBinaryOps(expr, binOps) :
    for x in binOps :
        x.expr1 = expr
        expr = x
    return expr

notExpr = astNode(sequence(optional(keyword("not")), compareExpr),
                  lambda x : x[1] if not x[0] else NotExpr(x[1]))

andExprTail = astNode(sequence(keyword("and"), notExpr),
                      lambda x : AndExpr(None, x[1]))
andExpr = modify(sequence(notExpr, zeroPlus(andExprTail)),
                 lambda x : foldBinaryOps(x[0], x[1]))

orExprTail = astNode(sequence(keyword("or"), andExpr),
                     lambda x : OrExpr(None, x[1]))
orExpr = modify(sequence(andExpr, zeroPlus(orExprTail)),
                lambda x : foldBinaryOps(x[0], x[1]))

staticExpr = astNode(sequence(keyword("static"), orExpr),
                     lambda x : StaticExpr(x[1]))

expression = choice(staticExpr, orExpr)


#
# type spec
#

typeSpec = modify(sequence(symbol(":"), expression), lambda x : x[1])
optTypeSpec = optional(typeSpec)


#
# statements
#

statement2 = lambda input : statement(input)

block = astNode(sequence(symbol("{"), zeroPlus(statement2), symbol("}")),
                lambda x : Block(x[1]))

labelDef = astNode(sequence(identifier, symbol(":")),
                   lambda x : Label(x[0]))

letOrRef = choice(modify(keyword("let"), lambda x : False),
                  modify(keyword("ref"), lambda x : True))

localBinding = astNode(sequence(letOrRef, identifier, optTypeSpec,
                                symbol("="), expression, semicolon),
                       lambda x : LocalBinding(x[0],x[1],x[2],x[4]))

assignment = astNode(sequence(expression, symbol("="), expression, semicolon),
                     lambda x : Assignment(x[0], x[2]))

gotoStatement = astNode(sequence(keyword("goto"), identifier, semicolon),
                        lambda x : Goto(x[1]))

returnStatement = astNode(sequence(keyword("return"), optExpression,
                                   semicolon),
                          lambda x : Return(x[1]))
parenCondition = modify(sequence(symbol("("), expression, symbol(")")),
                        lambda x : x[1])
elsePart = modify(sequence(keyword("else"), statement2),
                  lambda x : x[1])
ifStatement = astNode(sequence(keyword("if"), parenCondition,
                               statement2, optional(elsePart)),
                      lambda x : IfStatement(x[1],x[2],x[3]))

exprStatement = astNode(sequence(expression, semicolon),
                        lambda x : ExprStatement(x[0]))

whileStatement = astNode(sequence(keyword("while"), parenCondition,
                                  statement2),
                         lambda x : While(x[1], x[2]))

breakStatement = astNode(sequence(keyword("break"), semicolon),
                         lambda x : Break())
continueStatement = astNode(sequence(keyword("continue"), semicolon),
                            lambda x : Continue())

forStatement = astNode(sequence(keyword("for"), symbol("("),
                                identifier, optTypeSpec, keyword("in"),
                                expression, symbol(")"), statement2),
                       lambda x : For(x[2], x[3], x[5], x[7]))

statement = choice(block, labelDef, localBinding, assignment, ifStatement,
                   gotoStatement, returnStatement, exprStatement,
                   whileStatement, breakStatement, continueStatement,
                   forStatement)


#
# code
#

identifierList = listOf(identifier, comma)
typeVars = modify(sequence(symbol("["), identifierList, symbol("]")),
                  lambda x : x[1])
optTypeVars = modify(optional(typeVars), lambda x : [] if x is None else x)

valueArgument = astNode(sequence(identifier, optTypeSpec),
                        lambda x : ValueArgument(x[0],x[1]))
staticArgument = astNode(sequence(keyword("static"), expression),
                         lambda x : StaticArgument(x[1]))
formalArgument = choice(valueArgument, staticArgument)
formalArguments = modify(optional(listOf(formalArgument, comma)),
                         lambda x : [] if x is None else x)

byRef = modify(optional(keyword("ref")), lambda x : x is not None)

predicate = modify(sequence(keyword("if"), parenCondition),
                   lambda x : x[1])

code = astNode(sequence(optTypeVars, symbol("("), formalArguments,
                        symbol(")"), byRef, optTypeSpec,
                        optional(predicate), block),
               lambda x : Code(x[0],x[2],x[4],x[5],x[6],x[7]))


#
# top level items
#

valueRecordArg = astNode(sequence(identifier, typeSpec),
                         lambda x : ValueRecordArg(x[0], x[1]))
staticRecordArg = astNode(sequence(keyword("static"), expression),
                          lambda x : StaticRecordArg(x[1]))
recordArg = choice(valueRecordArg, staticRecordArg)
recordArgs = modify(optional(listOf(recordArg, comma)),
                    lambda x : [] if x is None else x)

record = astNode(sequence(keyword("record"), identifier, optTypeVars,
                          symbol("("), recordArgs, symbol(")"), semicolon),
                 lambda x : Record(x[1],x[2],x[4]))

procedure = astNode(sequence(keyword("def"), identifier, code),
                    lambda x : Procedure(x[1], x[2]))
overloadable = astNode(sequence(keyword("overloadable"), identifier,
                                semicolon),
                       lambda x : Overloadable(x[1]))
overload = astNode(sequence(keyword("overload"), identifier, code),
                   lambda x : Overload(x[1], x[2]))

topLevelItem = choice(record, procedure, overloadable, overload)


#
# import, export
#

importSpec = astNode(sequence(keyword("import"), dottedName, semicolon),
                     lambda x : Import(x[1]))
exportSpec = astNode(sequence(keyword("export"), dottedName, semicolon),
                     lambda x : Export(x[1]))

imports = zeroPlus(importSpec)
exports = zeroPlus(exportSpec)


#
# module
#

module = astNode(sequence(imports, exports, zeroPlus(topLevelItem)),
                 lambda x : Module(x[0], x[1], x[2]))


#
# parse
#

def parse(data, fileName) :
    tokens = lexer.tokenize(data, fileName)
    input = Input(tokens)
    result = module(input)
    if (result is Failure) or (input.pos < len(tokens)) :
        if input.maxPos == len(tokens) :
            location = Location(data, len(data), fileName)
        else :
            location = tokens[input.maxPos].location
        error("parse error", location=location)
    return result
