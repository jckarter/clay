from clay.parserlib import *
from clay import tokens as t
from clay import lexer
from clay.ast import *
from clay import linecol

#
# primitives
#

def token_type(klass) :
    def predicate(x) : return type(x) is klass
    return modify(condition(predicate), lambda x : x.value)

def token_value(klass, value) :
    def predicate(x) :
        return (type(x) is klass) and (x.value == value)
    return modify(condition(predicate), lambda x : x.value)

def symbol(s) : return token_value(t.Symbol, s)
def keyword(s) : return token_value(t.Keyword, s)

identifier = token_type(t.Identifier)

semicolon = symbol(";")
comma = symbol(",")


#
# literals
#

string_literal = modify(token_type(t.StringLiteral),
                        lambda x : StringLiteral(x))
char_literal = modify(token_type(t.CharLiteral), lambda x : CharLiteral(x))
int_literal = modify(token_type(t.IntLiteral), lambda x : IntLiteral(x))
bool_literal = choice(modify(keyword("true"), lambda x : BoolLiteral(True)),
                      modify(keyword("false"), lambda x : BoolLiteral(False)))
literal = choice(bool_literal, int_literal, char_literal, string_literal)


#
# atomic expr
#

expression2 = lambda input : expression(input)
expression_list = listof(expression2, comma)

array_expr = modify(sequence(symbol("["), expression_list, symbol("]")),
                    lambda x : ArrayExpr(x[1]))
tuple_expr = modify(sequence(symbol("("), expression_list, symbol(")")),
                    lambda x : TupleExpr(x[1]))
name_ref = modify(identifier, lambda x : NameRef(x))
atomic_expr = choice(array_expr, tuple_expr, name_ref, literal)


#
# suffix expr
#

index_suffix = modify(sequence(symbol("["), expression_list, symbol("]")),
                      lambda x : IndexExpr(None,x[1]))
call_suffix = modify(sequence(symbol("("), expression_list, symbol(")")),
                     lambda x : CallExpr(None,x[1]))
field_ref_suffix = modify(sequence(symbol("."), identifier),
                          lambda x : FieldRef(None,x[1]))

suffix = choice(index_suffix, call_suffix, field_ref_suffix)

def fold_suffixes(expr, suffixes) :
    for suffix in suffixes :
        suffix.expr = expr
        expr = suffix
    return expr

suffix_expr = modify(sequence(atomic_expr, zeroplus(suffix)),
                     lambda x : fold_suffixes(x[0], x[1]))


#
# expression
#

expression = suffix_expr


#
# statements
#

return_statement = modify(sequence(keyword("return"), expression, semicolon),
                          lambda x : ReturnStatement(x[1]))
assignment = modify(sequence(identifier, symbol("="), expression, semicolon),
                    lambda x : Assignment(x[0], x[2]))
local_variable_def = modify(sequence(keyword("var"), identifier,
                                     symbol("="), expression, semicolon),
                            lambda x : LocalVariableDef(x[1],x[3]))

statement2 = lambda input : statement(input)

if_condition = modify(sequence(symbol("("), expression, symbol(")")),
                      lambda x : x[1])

else_part = modify(sequence(keyword("else"), statement2),
                   lambda x : x[1])

if_statement = modify(sequence(keyword("if"), if_condition,
                               statement2, optional(else_part)),
                      lambda x : IfStatement(x[1],x[2],x[3]))
block = modify(sequence(symbol("{"), oneplus(statement2), symbol("}")),
               lambda x : Block(x[1]))

expr_statement = modify(sequence(expression, semicolon),
                        lambda x : x[0])

statement = choice(block, local_variable_def, assignment,
                   if_statement, return_statement, expr_statement)

#
# top level items
#

name_list = listof(identifier, comma)
type_vars = modify(sequence(symbol("["), name_list, symbol("]")),
                   lambda x : x[1])
opt_type_vars = modify(optional(type_vars), lambda x : [] if x is None else x)
field_def = modify(sequence(identifier, symbol(":"), expression),
                   lambda x : Field(x[0],x[2]))
field_list = listof(field_def, comma)
record_def = modify(sequence(keyword("record"), identifier, opt_type_vars,
                             symbol("{"), field_list, symbol("}")),
                    lambda x : RecordDef(x[1],x[2],x[4]))
variable_def = modify(sequence(keyword("var"), identifier,
                               symbol("="), expression, semicolon),
                      lambda x : VariableDef(x[1],x[3]))

opt_name_list = modify(optional(name_list), lambda x : [] if x is None else x)
procedure_def = modify(sequence(keyword("def"), identifier,
                                symbol("("), opt_name_list, symbol(")"),
                                block),
                       lambda x : ProcedureDef(x[1],x[3],x[5]))

top_level_item = choice(record_def, variable_def, procedure_def)


#
# program
#

program = modify(oneplus(top_level_item), lambda x : Program(x))


#
# parse
#

class ParseError(Exception) :
    def __init__(self, filename, line, column) :
        self.filename = filename
        self.line = line
        self.column = column

def parse(data, filename) :
    try :
        tokens = lexer.tokenize(data, filename)
    except lexer.LexerError, e :
        raise ParseError(e.filename, e.line, e.column)
    input = Input(tokens)
    result = program(input)
    if (result is Failure) or (input.pos < len(tokens)) :
        if input.max_pos == len(tokens) :
            pos = len(data)
        else :
            pos = tokens[input.max_pos].start
        line,column = linecol.locate_line_column(data, pos)
        raise ParseError(filename, line, column)
    return result
