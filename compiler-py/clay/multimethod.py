from clay.error import error

__all__ = ["multimethod"]

def multimethod(name, n=1, errorMessage=None, defaultProc=None) :
    mm = MultiMethod(name, n)
    if errorMessage is not None :
        mm.register(*((object,)*n))(lambda *args : error(errorMessage))
    if defaultProc is not None :
        mm.register(*((object,)*n))(defaultProc)
    return mm

class MultiMethod(object) :
    def __init__(self, name, n) :
        self.name = name
        self.n = n
        self.handlers_ = []
        self.cache_ = {}
    def register(self, *types) :
        assert len(types) == self.n
        def modifier(f) :
            self.handlers_.insert(0, (types, f))
            return self
        return modifier
    def __call__(self, *args) :
        checkedArgs = args[:self.n]
        key = tuple((type(x) for x in checkedArgs))
        handler = self.cache_.get(key)
        if handler is not None :
            return handler(*args)
        for types, f in self.handlers_ :
            if self.isMatch(checkedArgs, types) :
                self.cache_[key] = f
                return f(*args)
        error("no method instance of %s for %s" % (self.name, str(args)))
    def isMatch(self, args, types) :
        if len(args) != len(types) :
            return False
        for arg, type_ in zip(args, types) :
            if not isinstance(arg, type_) :
                return False
        return True
