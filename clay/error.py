__all__ = ["SourceError"]

class SourceError(Exception) :
    def __init__(self, error_message, data, offset, file_name) :
        self.error_message = error_message
        self.data = data
        self.offset = offset
        self.file_name = file_name

    def display(self) :
        lines = self.data.splitlines(True)
        line, column = locate_(lines, self.offset)
        display_context_(lines, line, column)
        print "error at %s:%d:%d: %s" % \
            (self.file_name, line+1, column, self.error_message)

def locate_(lines, offset) :
    line = 0
    for s in lines :
        if offset < len(s) :
            break
        offset -= len(s)
        line += 1
    column = offset
    return line, column

def display_context_(lines, line, column) :
    for i in range(5) :
        j = line + i -2
        if j < 0 : continue
        if j >= len(lines) : break
        print trim_line_ending_(lines[j])
        if j == line :
            print ("-" * column) + "^"

def trim_line_ending_(line) :
    if line.endswith("\r\n") : return line[:-2]
    if line.endswith("\r") : return line[:-1]
    if line.endswith("\n") : return line[:-1]
    return line
