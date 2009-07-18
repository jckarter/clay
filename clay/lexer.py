import re
from clay.parserlib import *
from clay.tokens import *
from clay import linecol

__all__ = ["LexerError", "tokenize"]

class LexerInput(Input) :
    def __init__(self, data) :
        super(LexerInput,self).__init__(data)

    def consume_string(self, s) :
        if not self.data.startswith(s, self.pos) :
            return Failure
        self.pos += len(s)
        self.update_max_pos()
        return s

    def consume_regex(self, regex) :
        match = regex.match(self.data, self.pos)
        if match is None :
            return Failure
        s = match.group()
        self.pos += len(s)
        self.update_max_pos()
        return s

def string_parser(s) :
    def parser_proc(input) :
        return input.consume_string(s)
    return parser_proc

def regex_parser(regex) :
    def parser_proc(input) :
        return input.consume_regex(regex)
    return parser_proc

def not_ahead(parser) :
    def parser_proc(input) :
        input.begin()
        result = parser(input)
        input.rollback()
        if result is Failure :
            return ""
        return Failure
    return parser_proc

def token(parser, klass) :
    def parser_proc(input) :
        start = input.pos
        result = parser(input)
        if result is Failure :
            return result
        end = input.pos
        return klass(result, start, end)
    return parser_proc


#
# symbol
#

symbols = ("( ) [ ] { } "
           + "<< >> | & "
           + "<= <> != >= == < > "
           + ": -> = ; , "
           + "+ - ~ . ^ ").split()
def symbols_regex() :
    return "|".join([re.escape(s) for s in symbols])
symbol = token(regex_parser(re.compile(symbols_regex())),
               klass=Symbol)


#
# keyword, identifier
#

keyword_list = ("record var def "
                + "break continue return if else while for in "
                + "or and not bitor bitand bitxor "
                + "true false ").split()
keyword_set = set(keyword_list)

ident_char1 = condition(lambda x : x.isalpha() or (x == "_"))
ident_char2 = condition(lambda x : x.isalpha() or x.isdigit() or (x == "_"))
keyword_ident1 = modify(sequence(ident_char1, zeroplus(ident_char2)),
                        lambda x : x[0] + "".join(x[1]))
def keyword_ident_factory(s, start, end) :
    if s in keyword_set :
        return Keyword(s, start, end)
    else :
        return Identifier(s, start, end)

keyword_identifier = token(keyword_ident1, klass=keyword_ident_factory)


#
# make_string_parser
#

def make_string_parser(delim, is_single, is_unicode) :
    def mk(x) :
        if is_unicode :
            return unichr(x)
        return chr(x)
    def conv(c) :
        assert type(c) is str
        if is_unicode :
            c = unichr(ord(c))
        return c
    simple_escape = choice(modify(literal("n"), lambda x : conv("\n")),
                           modify(literal("r"), lambda x : conv("\r")),
                           modify(literal("t"), lambda x : conv("\t")),
                           modify(literal("a"), lambda x : conv("\a")),
                           modify(literal("b"), lambda x : conv("\b")),
                           modify(literal("f"), lambda x : conv("\f")),
                           modify(literal("v"), lambda x : conv("\v")),
                           modify(literal("\\"), lambda x : conv("\\")),
                           modify(literal("'"), lambda x : conv("'")),
                           modify(literal("\""), lambda x : conv("\"")))
    octal_digit = condition(lambda c : c in "01234567")
    octal_escape = modify(many(octal_digit, min=1, max=3),
                          lambda x : mk(int("".join(x), 8)))
    hex_digit = condition(lambda c : c in "0123456789ABCDEFabcdef")
    hex_escape = modify(sequence(literal("x"), many(hex_digit,min=2,max=2)),
                        lambda x : mk(int("".join(x[1]),16)))
    unicode_escape1 = modify(sequence(literal("u"), many(hex_digit,min=4,max=4)),
                             lambda x : mk(int("".join(x[1]),16)))
    unicode_escape2 = modify(sequence(literal("U"), many(hex_digit,min=8,max=8)),
                             lambda x : mk(int("".join(x[1]),16)))
    escapes = [simple_escape, octal_escape, hex_escape]
    if is_unicode :
        escapes.extend([unicode_escape1, unicode_escape2])
    escape_char = sequence(literal("\\"), choice(*escapes))
    simple_set = delim + "\\"
    simple_char = modify(condition(lambda c : c not in simple_set),
                         lambda x : conv(x))
    string_char = choice(simple_char, escape_char)
    if is_single :
        parser = modify(sequence(literal(delim), string_char,
                                 literal(delim)),
                        lambda x : x[1])
    else :
        parser = modify(sequence(literal(delim), zeroplus(string_char),
                                 literal(delim)),
                        lambda x : "".join(x[1]))
    return parser


#
# string_literal, char_literal, bytes_literal, byte_literal
#

string_literal1 = make_string_parser("\"", is_single=False, is_unicode=True)
string_literal = token(string_literal1, klass=StringLiteral)

char_literal1 = make_string_parser("\'", is_single=True, is_unicode=True)
char_literal = token(char_literal1, klass=CharLiteral)

bytes_literal1 = make_string_parser("\"", is_single=False, is_unicode=False)
bytes_literal2 = modify(sequence(literal("b"), bytes_literal1),
                        lambda x : x[1])
bytes_literal = token(bytes_literal2, klass=BytesLiteral)

byte_literal1 = make_string_parser("\'", is_single=True, is_unicode=False)
byte_literal2 = modify(sequence(literal("b"), byte_literal1),
                       lambda x : x[1])
byte_literal = token(byte_literal2, klass=ByteLiteral)


#
# int_literal
#

hex_digit = condition(lambda c : c in "0123456789ABCDEFabcdef")
hex_int = modify(sequence(literal("0"), literal("x"), oneplus(hex_digit)),
                 lambda x : int("".join(x[2]), 16))

binary_digit = condition(lambda c : c in "01")
binary_int = modify(sequence(literal("0"), literal("b"),
                             oneplus(binary_digit)),
                    lambda x : int("".join(x[2]), 2))

octal_digit = condition(lambda c : c in "01234567")
octal_int = modify(sequence(literal("0"), oneplus(octal_digit)),
                   lambda x : int("".join(x[1]), 8))

decimal_digit = condition(lambda c : c in "0123456789")
decimal_int = modify(oneplus(decimal_digit), lambda x : int("".join(x)))

unsigned_int = choice(hex_int, binary_int, octal_int, decimal_int)
positive_int = modify(sequence(literal("+"), unsigned_int),
                      lambda x : x[1])
negative_int = modify(sequence(literal("-"), unsigned_int),
                      lambda x : -x[1])
int_literal = token(choice(negative_int, positive_int, unsigned_int),
                    klass=IntLiteral)


#
# space, single_line_comment, multi_line_comment
#

space = token(oneplus(condition(lambda x : x.isspace())),
              klass=Space)

end_of_line = condition(lambda c : c in "\r\n")
any_char = condition(lambda c : True)
slc_char = modify(sequence(not_ahead(end_of_line), any_char),
                         lambda x : x[1])
slc_string1 = modify(zeroplus(slc_char), lambda x : "".join(x))
slc_string2 = modify(sequence(string_parser("//"), slc_string1),
                     lambda x : x[1])
single_line_comment = token(slc_string2, klass=SingleLineComment)

mlc_begin = string_parser("/*")
mlc_end = string_parser("*/")
mlc_char = modify(sequence(not_ahead(mlc_end), any_char),
                  lambda x : x[1])
mlc_string1 = modify(zeroplus(mlc_char), lambda x : "".join(x))
mlc_string2 = modify(sequence(mlc_begin, mlc_string1, mlc_end),
                     lambda x : x[1])

multi_line_comment = token(mlc_string2, klass=MultiLineComment)


#
# one_token
#

one_token = choice(space, single_line_comment, multi_line_comment,
                   symbol, keyword_identifier,
                   string_literal, char_literal,
                   bytes_literal, byte_literal,
                   int_literal)


#
# LexerError, Lexer
#

class LexerError(Exception) :
    def __init__(self, filename, line, column) :
        self.filename = filename
        self.line = line
        self.column = column

def tokenize(data, filename, remove_space=True) :
    tokens = []
    input = LexerInput(data)
    while input.pos < len(data) :
        token = one_token(input)
        if token is Failure :
            line,column = linecol.locate_line_column(data, input.max_pos)
            raise LexerError(filename, line, column)
        tokens.append(token)
    def is_space(t) :
        return type(t) in [Space,SingleLineComment,MultiLineComment]
    if remove_space :
        tokens = [t for t in tokens if not is_space(t)]
    return tokens
