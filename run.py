import sys
from clay.error import SourceError
from clay.parser import parse
from clay.astprinter import ast_print

def main() :
    file_name = sys.argv[1]
    data = file(file_name).read()
    try :
        result = parse(data, file_name)
    except SourceError, e :
        e.display()
        return
    ast_print(result, 100)

if __name__ == "__main__" :
    main()
