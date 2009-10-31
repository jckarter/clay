import re
from clay.parserlib import *
from clay.tokens import *
from clay.error import Location, error

__all__ = ["tokenize"]

class LexerInput(Input) :
    def __init__(self, data, fileName) :
        super(LexerInput,self).__init__(data)
        self.fileName = fileName

    def consumeString(self, s) :
        if not self.data.startswith(s, self.pos) :
            return Failure
        self.pos += len(s)
        self.updateMaxPos()
        return s

    def consumeRegex(self, regex) :
        match = regex.match(self.data, self.pos)
        if match is None :
            return Failure
        s = match.group()
        self.pos += len(s)
        self.updateMaxPos()
        return s

def stringParser(s) :
    def parserProc(input) :
        return input.consumeString(s)
    return parserProc

def regexParser(regex) :
    def parserProc(input) :
        return input.consumeRegex(regex)
    return parserProc

def notAhead(parser) :
    def parserProc(input) :
        input.begin()
        result = parser(input)
        input.rollback()
        if result is Failure :
            return ""
        return Failure
    return parserProc

def token(parser, klass) :
    def parserProc(input) :
        start = input.pos
        result = parser(input)
        if result is Failure :
            return result
        tok = klass(result)
        tok.location = Location(input.data, start, input.fileName)
        return tok
    return parserProc


#
# symbol
#

symbols = ("( ) [ ] { } " +
           ": ; , . " +
           "== != <= < >= > " +
           "+ - * / % " +
           "= & ^").split()
def symbolsRegex() :
    return "|".join([re.escape(s) for s in symbols])
symbol = token(regexParser(re.compile(symbolsRegex())),
               klass=Symbol)


#
# keyword, identifier
#

keywordList = ("import export predicate instance record " +
               "overloadable overload external " +
               "static var ref and or not " +
               "if else goto return while break continue for in " +
               "true false").split()
keywordSet = set(keywordList)

identChar1 = condition(lambda x : x.isalpha() or (x == "_"))
identChar2 = condition(lambda x : x.isalpha() or x.isdigit() or (x in "_?"))
keywordIdentStr = modify(sequence(identChar1, zeroPlus(identChar2)),
                         lambda x : x[0] + "".join(x[1]))
def keywordIdentFactory(s) :
    return Keyword(s) if s in keywordSet else Identifier(s)

keywordIdentifier = token(keywordIdentStr, klass=keywordIdentFactory)


#
# makeStringParser
#

def makeStringParser(delim, isSingle, isUnicode) :
    def mk(x) :
        if isUnicode :
            return unichr(x)
        return chr(x)
    def conv(c) :
        assert type(c) is str
        if isUnicode :
            c = unichr(ord(c))
        return c
    simpleEscape = choice(modify(literal("n"), lambda x : conv("\n")),
                          modify(literal("r"), lambda x : conv("\r")),
                          modify(literal("t"), lambda x : conv("\t")),
                          modify(literal("a"), lambda x : conv("\a")),
                          modify(literal("b"), lambda x : conv("\b")),
                          modify(literal("f"), lambda x : conv("\f")),
                          modify(literal("v"), lambda x : conv("\v")),
                          modify(literal("\\"), lambda x : conv("\\")),
                          modify(literal("'"), lambda x : conv("'")),
                          modify(literal("\""), lambda x : conv("\"")))
    octalDigit = condition(lambda c : c in "01234567")
    octalEscape = modify(many(octalDigit, min=1, max=3),
                         lambda x : mk(int("".join(x), 8)))
    hexDigit = condition(lambda c : c in "0123456789ABCDEFabcdef")
    hexEscape = modify(sequence(literal("x"), many(hexDigit,min=2,max=2)),
                       lambda x : mk(int("".join(x[1]),16)))
    unicodeEscape1 = modify(sequence(literal("u"), many(hexDigit,min=4,max=4)),
                            lambda x : mk(int("".join(x[1]),16)))
    unicodeEscape2 = modify(sequence(literal("U"), many(hexDigit,min=8,max=8)),
                            lambda x : mk(int("".join(x[1]),16)))
    escapes = [simpleEscape, octalEscape, hexEscape]
    if isUnicode :
        escapes.extend([unicodeEscape1, unicodeEscape2])
    escapeChar = modify(sequence(literal("\\"), choice(*escapes)),
                        lambda x : x[1])
    simpleSet = delim + "\\"
    simpleChar = modify(condition(lambda c : c not in simpleSet),
                        lambda x : conv(x))
    stringChar = choice(simpleChar, escapeChar)
    if isSingle :
        parser = modify(sequence(literal(delim), stringChar,
                                 literal(delim)),
                        lambda x : x[1])
    else :
        parser = modify(sequence(literal(delim), zeroPlus(stringChar),
                                 literal(delim)),
                        lambda x : "".join(x[1]))
    return parser


#
# stringLiteral, charLiteral, bytesLiteral, byteLiteral
#

stringLiteral1 = makeStringParser("\"", isSingle=False, isUnicode=True)
stringLiteral = token(stringLiteral1, klass=StringLiteral)

charLiteral1 = makeStringParser("\'", isSingle=True, isUnicode=True)
charLiteral = token(charLiteral1, klass=CharLiteral)

bytesLiteral1 = makeStringParser("\"", isSingle=False, isUnicode=False)
bytesLiteral2 = modify(sequence(literal("b"), bytesLiteral1),
                       lambda x : x[1])
bytesLiteral = token(bytesLiteral2, klass=BytesLiteral)

byteLiteral1 = makeStringParser("\'", isSingle=True, isUnicode=False)
byteLiteral2 = modify(sequence(literal("b"), byteLiteral1),
                      lambda x : x[1])
byteLiteral = token(byteLiteral2, klass=ByteLiteral)


#
# intLiteral
#

hexDigit = condition(lambda c : c in "0123456789ABCDEFabcdef")
hexInt = modify(sequence(literal("0"), literal("x"), onePlus(hexDigit)),
                lambda x : int("".join(x[2]), 16))

binaryDigit = condition(lambda c : c in "01")
binaryInt = modify(sequence(literal("0"), literal("b"),
                            onePlus(binaryDigit)),
                   lambda x : int("".join(x[2]), 2))

octalDigit = condition(lambda c : c in "01234567")
octalInt = modify(sequence(literal("0"), onePlus(octalDigit)),
                  lambda x : int("".join(x[1]), 8))

decimalDigit = condition(lambda c : c in "0123456789")
decimalInt = modify(onePlus(decimalDigit), lambda x : int("".join(x)))

unsignedInt = choice(hexInt, binaryInt, octalInt, decimalInt)
positiveInt = modify(sequence(literal("+"), unsignedInt),
                     lambda x : x[1])
negativeInt = modify(sequence(literal("-"), unsignedInt),
                     lambda x : -x[1])
intLiteral = token(choice(negativeInt, positiveInt, unsignedInt),
                   klass=IntLiteral)


#
# floatLiteral
#

optSign = modify(optional(choice(literal("+"), literal("-"))),
                 lambda x : "" if x is None else x)
integralPart = modify(onePlus(decimalDigit), lambda x : "".join(x))
decimalPart = modify(sequence(literal("."), integralPart),
                     lambda x : "".join(x))
floatValue = modify(sequence(optSign, integralPart, decimalPart),
                    lambda x : float("".join(x)))
floatLiteral = token(floatValue, klass=FloatLiteral)


#
# literalSuffix
#

literalSuffix1 = modify(sequence(literal("#"), keywordIdentStr),
                        lambda x : x[1])

literalSuffix = token(literalSuffix1, klass=LiteralSuffix)


#
# space, singleLineComment, multiLineComment
#

space = token(onePlus(condition(lambda x : x.isspace())),
              klass=Space)

endOfLine = condition(lambda c : c in "\r\n")
anyChar = condition(lambda c : True)
slcChar = modify(sequence(notAhead(endOfLine), anyChar),
                 lambda x : x[1])
slcString1 = modify(zeroPlus(slcChar), lambda x : "".join(x))
slcString2 = modify(sequence(stringParser("//"), slcString1),
                    lambda x : x[1])
singleLineComment = token(slcString2, klass=SingleLineComment)

mlcBegin = stringParser("/*")
mlcEnd = stringParser("*/")
mlcChar = modify(sequence(notAhead(mlcEnd), anyChar),
                 lambda x : x[1])
mlcString1 = modify(zeroPlus(mlcChar), lambda x : "".join(x))
mlcString2 = modify(sequence(mlcBegin, mlcString1, mlcEnd),
                    lambda x : x[1])

multiLineComment = token(mlcString2, klass=MultiLineComment)


#
# oneToken
#

oneToken = choice(space, singleLineComment, multiLineComment,
                  symbol, keywordIdentifier,
                  stringLiteral, charLiteral,
                  bytesLiteral, byteLiteral,
                  floatLiteral, intLiteral, literalSuffix)


#
# Lexer
#

def tokenize(data, fileName, remove_space=True) :
    tokens = []
    input = LexerInput(data, fileName)
    while input.pos < len(data) :
        token = oneToken(input)
        if token is Failure :
            location = Location(data, input.maxPos, fileName)
            error("invalid token", location=location)
        tokens.append(token)
    def is_space(t) :
        return type(t) in [Space,SingleLineComment,MultiLineComment]
    if remove_space :
        tokens = [t for t in tokens if not is_space(t)]
    return tokens
