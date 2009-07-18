#
# xprint.py
#
# Author: KS Sreeram (email: sreeram@tachyontech.net)
# License: MIT license
#
# Copyright 2009, KS Sreeram
#
# This is a pretty printer which can be extended to support custom
# objects.  If you've ever wanted to extend the 'pprint' module to support 
# custom types, then this module is for you!
#


__all__ = ["xprint", "XObject", "XField", "XSymbol", "xregister"]

#
# xprint
#

def xprint(x, columns=80) :
    buf = PrintBuffer()
    _xprint(x, buf, max_offset=columns)
    for line in buf.lines() :
        print line


#
# _xprint : (x,buf,max_offset) -> overflowed
#

def _xprint(x, buf, max_offset) :
    t = type(x)
    handler = _handlers.get(t)
    if handler is None :
        converter = _converters.get(t)
        if converter is None :
            return _xprint_atom(x, buf, max_offset)
        data = converter(x)
        return _xprint(data, buf, max_offset)
    return handler(x, buf, max_offset)


#
# XObject, XField
#

class XObject(object) :
    def __init__(self, name, *fields) :
        self.name = name
        self.fields = fields

class XField(object) :
    def __init__(self, name, value, separator="=") :
        self.name = name
        self.value = value
        self.separator = separator

class XSymbol(object) :
    def __init__(self, name) :
        self.name = name


#
# _xprint handlers
#

def _xprint_atom(x, buf, max_offset) :
    buf.add(repr(x))
    return buf.last_line_length() >= max_offset

def _xprint_list(x, buf, max_offset) :
    return _xprint_container("[", x, "]", buf, max_offset)

def _xprint_tuple(x, buf, max_offset) :
    return _xprint_container("(", x, ")", buf, max_offset)

def _xprint_xobject(x, buf, max_offset) :
    return _xprint_container(x.name + "(", x.fields, ")", buf, max_offset)

def _xprint_xfield(x, buf, max_offset) :
    buf.add(x.name)
    buf.add(x.separator)
    return _xprint(x.value, buf, max_offset)

def _xprint_xsymbol(x, buf, max_offset) :
    buf.add(x.name)
    return buf.last_line_length() >= max_offset


#
# converters, register
#

_converters = {}

def xregister(type, converter) :
    _converters[type] = converter


#
# init_handlers
#

_handlers = {}

def init_handlers() :
    for t in (type(None), bool, int, long, float,
              str, unicode) :
        _handlers[t] = _xprint_atom
    _handlers[list] = _xprint_list
    _handlers[tuple] = _xprint_tuple
    _handlers[XObject] = _xprint_xobject
    _handlers[XField] = _xprint_xfield
    _handlers[XSymbol] = _xprint_xsymbol
init_handlers()


#
# _xprint_container
#

def _xprint_container(prefix, items, suffix, buf, max_offset) :
    nlines_initial = buf.line_count()
    initial_last_len = buf.last_line_length()
    buf.add(prefix)
    saved_indent = buf.indent()
    buf.set_indent(buf.last_line_length())
    j = 0
    for i,item in enumerate(items) :
        if i > 0 :
            buf.add(",")
            if child_overflowed :
                buf.newline()
                j = 0
            else :
                buf.add(" ")
                j += 1
        checkpoint = buf.checkpoint()
        child_overflowed = _xprint(item, buf, max_offset)
        if child_overflowed :
            wrap = False
            if j > 0 : wrap = True
            if (i == 0) and (len(prefix) > 2) :
                wrap = True
                buf.set_indent(initial_last_len + 2)
            if wrap :
                buf.revert(checkpoint)
                buf.newline()
                j = 0
                child_overflowed = _xprint(item, buf, max_offset)
    buf.add(suffix)
    buf.set_indent(saved_indent)
    overflowed = (buf.line_count() > nlines_initial) or \
        (buf.last_line_length() >= max_offset)
    return overflowed


#
# PrintBuffer
#

class PrintBuffer(object) :
    def __init__(self) :
        self.lines_ = [[]]
        self.indent_ = 0
        self.last_line_length_ = 0

    def indent(self) :
        return self.indent_

    def set_indent(self, x) :
        self.indent_ = x

    def last_line_length(self) :
        return self.last_line_length_

    def line_count(self) :
        return len(self.lines_)

    def lines(self) :
        for line in self.lines_ :
            yield "".join(line)

    def add(self, s) :
        self.lines_[-1].append(s)
        self.last_line_length_ += len(s)

    def newline(self) :
        line = []
        self.last_line_length_ = 0
        if self.indent_ > 0 :
            line.append(" " * self.indent_)
            self.last_line_length_ += self.indent_
        self.lines_.append(line)

    def checkpoint(self) :
        nlines = len(self.lines_)
        nitems = len(self.lines_[-1])
        return (nlines, nitems, self.last_line_length_)

    def revert(self, checkpoint) :
        nlines, nitems, self.last_line_length_ = checkpoint
        del self.lines_[nlines:]
        del self.lines_[-1][nitems:]



#
# test code
#

class Person(object) :
    def __init__(self, name, age) :
        self.name = name
        self.age = age

def test1() :
    def convert_person(x) :
        return XObject("Person", x.name, x.age)
    xregister(Person, convert_person)
    xprint(Person("Stepanov", 40))
    def convert_person(x) :
        return XObject("Person", x.name, XField("age",x.age))
    xregister(Person, convert_person)
    xprint(Person("Stepanov", 40))

class BinaryOp(object) :
    def __init__(self, expr1, expr2) :
        self.expr1 = expr1
        self.expr2 = expr2

class UnaryOp(object) :
    def __init__(self, expr) :
        self.expr = expr

class Add(BinaryOp) : pass
class Subtract(BinaryOp) : pass
class Multiply(BinaryOp) : pass
class Divide(BinaryOp) : pass
class Negate(UnaryOp) : pass
class SquareRoot(UnaryOp) : pass

class Variable(object) :
    def __init__(self, name) :
        self.name = name

def test2():
    for t in (Add,Subtract,Multiply,Divide) :
        xregister(t, lambda x : XObject(type(x).__name__, x.expr1, x.expr2))

    for t in (Negate,SquareRoot) :
        xregister(t, lambda x : XObject(type(x).__name__, x.expr))

    xregister(Variable, lambda x : XSymbol(x.name))

    a,b,c = Variable('a'), Variable('b'), Variable('c')

    sroot = SquareRoot(Subtract(Multiply(b,b), Multiply(4,Multiply(a,c))))
    root1 = Divide(Add(Negate(b),sroot), Multiply(2,a))
    root2 = Divide(Subtract(Negate(b),sroot), Multiply(2,a))

    quadratic_roots = [root1, root2]

    print "#################################"
    print "#### xprint, columns=80 (default)"
    print "#################################"
    print ""
    xprint(quadratic_roots)
    print ""

    print "#################################"
    print "#### xprint, columns=50"
    print "#################################"
    print ""
    xprint(quadratic_roots, columns=50)
    print ""

if __name__ == "__main__" :
    test1()
    test2()
