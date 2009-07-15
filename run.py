import sys
from clay import lexer

def main() :
    filename = sys.argv[1]
    data = file(filename).read()
    try :
        tokens = lexer.tokenize(data, filename)
    except lexer.LexerError, e :
        print "lexer error at %s:%d:%d" % (e.filename,e.line,e.column)
    print tokens

main()
