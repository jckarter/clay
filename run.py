import sys
from clay import parser
from clay.astprinter import ast_print

def main() :
    filename = sys.argv[1]
    data = file(filename).read()
    try :
        result = parser.parse(data, filename)
    except parser.ParseError, e :
        print "parser error at %s:%d:%d" % (e.filename,e.line,e.column)
        return
    ast_print(result)

main()
