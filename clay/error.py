__all__ = ["Location", "raiseError", "SourceError"]

class Location(object) :
    def __init__(self, data, offset, fileName) :
        self.data = data
        self.offset = offset
        self.fileName = fileName

def raiseError(errorMessage, location) :
    raise SourceError(errorMessage, location)

class SourceError(Exception) :
    def __init__(self, errorMessage, location) :
        self.errorMessage = errorMessage
        self.location = location

    def display(self) :
        if self.location is None :
            print "error: %s" % self.errorMessage
            return
        lines = self.location.data.splitlines(True)
        line, column = locate(lines, self.location.offset)
        displayContext(lines, line, column)
        print "error at %s:%d:%d: %s" % \
            (self.location.fileName, line+1, column, self.errorMessage)

def locate(lines, offset) :
    line = 0
    for s in lines :
        if offset < len(s) :
            break
        offset -= len(s)
        line += 1
    column = offset
    return line, column

def displayContext(lines, line, column) :
    for i in range(5) :
        j = line + i -2
        if j < 0 : continue
        if j >= len(lines) : break
        print trimLineEnding(lines[j])
        if j == line :
            print ("-" * column) + "^"

def trimLineEnding(line) :
    if line.endswith("\r\n") : return line[:-2]
    if line.endswith("\r") : return line[:-1]
    if line.endswith("\n") : return line[:-1]
    return line
