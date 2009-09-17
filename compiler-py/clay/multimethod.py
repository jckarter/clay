from clay.error import error

__all__ = ["multimethod"]

def multimethod(n=1, errorMessage=None, defaultProc=None) :
    if errorMessage is not None :
        assert defaultProc is None
        defaultProc = lambda *args : error(errorMessage)
    return MultiMethod(n, defaultProc)

class MultiMethod(object) :
    def __init__(self, n, defaultProc) :
        self.registry_ = {}
        self.n_ = n
        self.defaultProc_ = defaultProc

    def addHandler(self, f, *types) :
        self.registry_[types] = f

    def register(self, *types) :
        def modifier(f) :
            self.addHandler(f, *types)
            return self
        return modifier

    def getHandler(self, *types) :
        return self.registry_.get(types, self.defaultProc_)

    def __call__(self, *args) :
        types = tuple(type(x) for x in args[:self.n_])
        handler = self.registry_.get(types, self.defaultProc_)
        return handler(*args)
