__all__ = ["multimethod"]

class DummyDefault(object) : pass

class DefaultProc(object) :
    def __init__(self, proc) :
        self.proc = proc

def multimethod(n=1, default=DummyDefault, defaultProc=DummyDefault) :
    assert (default is DummyDefault) or (defaultProc is DummyDefault)
    if default is not DummyDefault :
        defaultProc = lambda *args : default
    elif defaultProc is not DummyDefault :
        pass
    else :
        defaultProc = None
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

    def __call__(self, *args) :
        types = tuple(type(x) for x in args[:self.n_])
        handler = self.registry_.get(types, self.defaultProc_)
        return handler(*args)
