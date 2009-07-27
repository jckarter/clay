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
    _xprint(x, buf, maxOffset=columns)
    for line in buf.lines() :
        print line


#
# _xprint : (x,buf,maxOffset) -> overflowed
#

def _xprint(x, buf, maxOffset) :
    t = type(x)
    handler = _handlers.get(t)
    if handler is None :
        converter = _converters.get(t)
        if converter is None :
            return _xprintAtom(x, buf, maxOffset)
        data = converter(x)
        return _xprint(data, buf, maxOffset)
    return handler(x, buf, maxOffset)


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

def _xprintAtom(x, buf, maxOffset) :
    buf.add(repr(x))
    return buf.lastLineLength() >= maxOffset

def _xprintList(x, buf, maxOffset) :
    return _xprintContainer("[", x, "]", buf, maxOffset)

def _xprintTuple(x, buf, maxOffset) :
    return _xprintContainer("(", x, ")", buf, maxOffset)

def _xprintXObject(x, buf, maxOffset) :
    return _xprintContainer(x.name + "(", x.fields, ")", buf, maxOffset)

def _xprintXField(x, buf, maxOffset) :
    buf.add(x.name)
    buf.add(x.separator)
    return _xprint(x.value, buf, maxOffset)

def _xprintXSymbol(x, buf, maxOffset) :
    buf.add(x.name)
    return buf.lastLineLength() >= maxOffset


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
        _handlers[t] = _xprintAtom
    _handlers[list] = _xprintList
    _handlers[tuple] = _xprintTuple
    _handlers[XObject] = _xprintXObject
    _handlers[XField] = _xprintXField
    _handlers[XSymbol] = _xprintXSymbol
init_handlers()


#
# _xprintContainer
#

def _xprintContainer(prefix, items, suffix, buf, maxOffset) :
    nLinesInitial = buf.lineCount()
    initialLastLen = buf.lastLineLength()
    buf.add(prefix)
    savedIndent = buf.indent()
    buf.setIndent(buf.lastLineLength())
    j = 0
    for i,item in enumerate(items) :
        if i > 0 :
            buf.add(",")
            if childOverflowed :
                buf.newline()
                j = 0
            else :
                buf.add(" ")
                j += 1
        checkpoint = buf.checkpoint()
        childOverflowed = _xprint(item, buf, maxOffset)
        if childOverflowed :
            wrap = False
            if j > 0 : wrap = True
            if (i == 0) and (len(prefix) > 2) :
                wrap = True
                buf.setIndent(initialLastLen + 2)
            if wrap :
                buf.revert(checkpoint)
                buf.newline()
                j = 0
                childOverflowed = _xprint(item, buf, maxOffset)
    buf.add(suffix)
    buf.setIndent(savedIndent)
    overflowed = (buf.lineCount() > nLinesInitial) or \
        (buf.lastLineLength() >= maxOffset)
    return overflowed


#
# PrintBuffer
#

class PrintBuffer(object) :
    def __init__(self) :
        self.lines_ = [[]]
        self.indent_ = 0
        self.lastLineLength_ = 0

    def indent(self) :
        return self.indent_

    def setIndent(self, x) :
        self.indent_ = x

    def lastLineLength(self) :
        return self.lastLineLength_

    def lineCount(self) :
        return len(self.lines_)

    def lines(self) :
        for line in self.lines_ :
            yield "".join(line)

    def add(self, s) :
        self.lines_[-1].append(s)
        self.lastLineLength_ += len(s)

    def newline(self) :
        line = []
        self.lastLineLength_ = 0
        if self.indent_ > 0 :
            line.append(" " * self.indent_)
            self.lastLineLength_ += self.indent_
        self.lines_.append(line)

    def checkpoint(self) :
        nLines = len(self.lines_)
        nItems = len(self.lines_[-1])
        return (nLines, nItems, self.lastLineLength_)

    def revert(self, checkpoint) :
        nLines, nItems, self.lastLineLength_ = checkpoint
        del self.lines_[nLines:]
        del self.lines_[-1][nItems:]



#
# test code
#

class Person(object) :
    def __init__(self, name, age) :
        self.name = name
        self.age = age

def test1() :
    def convertPerson(x) :
        return XObject("Person", x.name, x.age)
    xregister(Person, convertPerson)
    xprint(Person("Stepanov", 40))
    def convertPerson(x) :
        return XObject("Person", x.name, XField("age",x.age))
    xregister(Person, convertPerson)
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

    quadraticRoots = [root1, root2]

    print "#################################"
    print "#### xprint, columns=80 (default)"
    print "#################################"
    print ""
    xprint(quadraticRoots)
    print ""

    print "#################################"
    print "#### xprint, columns=50"
    print "#################################"
    print ""
    xprint(quadraticRoots, columns=50)
    print ""

if __name__ == "__main__" :
    test1()
    test2()
