__all__ = ["Location", "CompilerError", "contextPush", "contextPop",
           "error", "ensure"]

class Location(object) :
    def __init__(self, data, offset, fileName) :
        self.data = data
        self.offset = offset
        self.fileName = fileName

class CompilerError(Exception) :
    def __init__(self, errorMessage, location) :
        self.errorMessage = errorMessage
        self.location = location

    def display(self) :
        displayError(self)

_errorContext = []

def contextPush(thing) :
    _errorContext.append(thing)

def contextPop() :
    _errorContext.pop()

def error(msg, **kwargs) :
    location = kwargs.get("location")
    if location is None :
        location = contextLocation()
    raise CompilerError(msg, location)

def ensure(cond, msg) :
    if not cond :
        error(msg)



#
# utility procedures
#

def contextLocation() :
    for thing in reversed(_errorContext) :
        if type(thing) is Location :
            return thing
        if hasattr(thing, "location") :
            if thing.location is not None :
                return thing.location
    return None

def displayError(err) :
    if err.location is None :
        print "error: %s" % err.errorMessage
        return
    lines = err.location.data.splitlines(True)
    line, column = locate(lines, err.location.offset)
    displayContext(lines, line, column)
    print "error at %s:%d:%d: %s" % \
        (err.location.fileName, line+1, column, err.errorMessage)

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
