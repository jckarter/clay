class Token(object) :
    def __init__(self, value) :
        self.value = value
        self.location = None
    def __str__(self) :
        return "%s(%s)" % (type(self).__name__, repr(self.value))
    def __repr__(self) :
        return str(self)

class Symbol(Token) : pass
class Keyword(Token) : pass
class Identifier(Token) : pass
class StringLiteral(Token) : pass
class CharLiteral(Token) : pass
class BytesLiteral(Token) : pass
class ByteLiteral(Token) : pass
class IntLiteral(Token) : pass
class FloatLiteral(Token) : pass
class DoubleLiteral(Token) : pass
class Space(Token) : pass
class SingleLineComment(Token) : pass
class MultiLineComment(Token) : pass
