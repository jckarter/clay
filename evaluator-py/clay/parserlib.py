__all__ = ["Failure", "Input", "zeroPlus", "choice",
           "sequence", "optional", "modify", "onePlus", "many", "listOf",
           "condition", "literal"]

#
# parser input
#

class Failure(object) : pass

class Input(object) :
    def __init__(self, data, pos=0) :
        self.data = data
        self.pos = pos
        self.maxPos = pos
        self.saved_ = []

    def next(self) :
        if self.pos >= len(self.data) :
            return Failure
        item = self.data[self.pos]
        self.pos += 1
        return item

    def begin(self) :
        self.saved_.append(self.pos)

    def commit(self) :
        self.saved_.pop()
        self.updateMaxPos()

    def rollback(self) :
        self.pos = self.saved_.pop()

    def updateMaxPos(self) :
        self.maxPos = max(self.maxPos, self.pos)


#
# parser combinators
#

def zeroPlus(parser) :
    def parserProc(input) :
        results = []
        while True :
            result = parser(input)
            if result is Failure :
                break
            results.append(result)
        return results
    return parserProc

def choice(*parsers) :
    def parserProc(input) :
        for parser in parsers :
            result = parser(input)
            if result is not Failure :
                return result
        return Failure
    return parserProc

def sequence(*parsers) :
    def parserProc(input) :
        results = []
        input.begin()
        for parser in parsers :
            result = parser(input)
            if result is Failure :
                input.rollback()
                return Failure
            results.append(result)
        input.commit()
        return results
    return parserProc

def optional(parser, default=None) :
    def parserProc(input) :
        result = parser(input)
        if result is Failure :
            result = default
        return result
    return parserProc

def modify(parser, modifier) :
    def parserProc(input) :
        result = parser(input)
        if result is not Failure :
            result = modifier(result)
        return result
    return parserProc

def onePlus(parser) :
    return modify(sequence(parser, zeroPlus(parser)),
                  lambda x : fixList(x))

def many(parser, min, max) :
    def parserProc(input) :
        input.begin()
        results = []
        while True :
            if len(results) == max :
                break
            result = parser(input)
            if result is Failure :
                break
            results.append(result)
        if len(results) < min :
            input.rollback()
            return Failure
        input.commit()
        return results
    return parserProc

def listOf(parser, separator) :
    tail = modify(sequence(separator, parser), lambda x : x[1])
    return modify(sequence(parser, zeroPlus(tail)), lambda x : fixList(x))

def fixList(headTail) :
    head = headTail[0]
    body = headTail[1]
    body.insert(0, head)
    return body

def condition(cond) :
    def parserProc(input) :
        input.begin()
        item = input.next()
        if (item is Failure) or (not cond(item)) :
            input.rollback()
            return Failure
        input.commit()
        return item
    return parserProc

def literal(lit) :
    return condition(lambda x : x == lit)
