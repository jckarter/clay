import sys
from clay import parser
from clay.astprinter import ast_print

def main() :
    file_name = sys.argv[1]
    data = file(file_name).read()
    try :
        result = parser.parse(data, file_name)
    except parser.ParseError, e :
        print "parser error at %s:%d:%d" % (e.file_name,e.line,e.column)
        return
    ast_print(result)

if __name__ == "__main__" :
    main()
