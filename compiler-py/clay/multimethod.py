from clay.error import error

__all__ = ["multimethod"]

def multimethod(n=1, errorMessage=None, defaultProc=None) :
    return MultiMethod(n, defaultProc, errorMessage)

class MultiMethod(object) :
    def __init__(self, n, defaultProc, errorMessage) :
        self.registry_ = {}
        self.n_ = n
        self.defaultProc_ = defaultProc
        self.errorMessage_ = errorMessage

    def getDefaultProc(self) : return self.defaultProc_
    def getErrorMessage(self) : return self.errorMessage_

    def getHandler(self, *types) :
        return self.registry_.get(types)

    def addHandler(self, f, *types) :
        self.registry_[types] = f

    def register(self, *types) :
        def modifier(f) :
            self.addHandler(f, *types)
            return self
        return modifier

    def __call__(self, *args) :
        types = tuple(type(x) for x in args[:self.n_])
        handler = self.registry_.get(types)
        if handler is not None :
            return handler(*args)
        if self.defaultProc_ is not None :
            return self.defaultProc_(*args)
        if self.errorMessage_ is not None :
            error(self.errorMessage_)
        assert False, "no handler for types: %s" % str(types)
