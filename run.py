import sys
from clay.error import SourceError
from clay.parser import parse
from clay.astprinter import astPrint

def main() :
    fileName = sys.argv[1]
    data = file(fileName).read()
    try :
        result = parse(data, fileName)
    except SourceError, e :
        e.display()
        return
    astPrint(result, 100)

if __name__ == "__main__" :
    main()
