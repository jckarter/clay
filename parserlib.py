__all__ = ["Input", "zeroplus", "choice", "sequence", "modify",
           "oneplus", "many", "listof", "condition", "literal"]

#
# parser input
#

class Input(object) :
    def __init__(self, data, pos=0) :
        self.data = data
        self.pos = pos
        self.max_pos = pos
        self.saved_ = []

    def next(self) :
        if self.pos >= len(self.data) :
            return None
        self.max_pos = max(self.max_pos, self.pos)
        item = self.data[self.pos]
        self.pos += 1
        return item

    def begin(self) :
        self.saved_.append(self.pos)

    def commit(self) :
        self.saved_.pop()

    def rollback(self) :
        self.pos = self.saved_.pop()


#
# parser combinators
#

def zeroplus(parser) :
    def parser_proc(input) :
        results = []
        while True :
            result = parser(input)
            if result is None :
                break
            results.append(result)
        return results
    return parser_proc

def choice(*parsers) :
    def parser_proc(input) :
        for parser in parsers :
            result = parser(input)
            if result is not None :
                return result
        return None
    return parser_proc

def sequence(*parsers) :
    def parser_proc(input) :
        results = []
        input.begin()
        for parser in parsers :
            result = parser(input)
            if result is None :
                input.rollback()
                return None
            results.append(result)
        input.commit()
        return results
    return parser_proc

def modify(parser, modifier) :
    def parser_proc(input) :
        result = parser(input)
        if result is not None :
            result = modifier(result)
        return result
    return parser_proc

def oneplus(parser) :
    return modify(sequence(parser, zeroplus(parser)),
                  lambda x : fixlist(x))

def many(parser, min, max) :
    def parser_proc(input) :
        input.begin()
        results = []
        while True :
            if len(results) == max :
                break
            result = parser(input)
            if result is None :
                break
            results.append(result)
        if len(results) < min :
            input.rollback()
            return None
        input.commit()
        return results
    return parser_proc

def listof(parser, separator) :
    tail = modify(sequence(separator, parser), lambda x : x[1])
    return modify(sequence(parser, zeroplus(tail)), lambda x : fixlist(x))

def fixlist(head_tail) :
    head = head_tail[0]
    body = head_tail[1]
    body.insert(0, head)
    return body

def condition(cond) :
    def parser_proc(input) :
        input.begin()
        item = input.next()
        if (item is None) or (not cond(item)) :
            input.rollback()
            return None
        input.commit()
        return item
    return parser_proc

def literal(lit) :
    return condition(lambda x : x == lit)
