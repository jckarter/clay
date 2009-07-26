__all__ = ["multimethod"]

class DummyDefault(object) : pass

class DefaultProc(object) :
    def __init__(self, proc) :
        self.proc = proc

def multimethod(n=1, default=DummyDefault, default_proc=DummyDefault) :
    assert (default is DummyDefault) or (default_proc is DummyDefault)
    if default is not DummyDefault :
        default_proc = lambda *args : default
    elif default_proc is not DummyDefault :
        pass
    else :
        default_proc = None
    return MultiMethod(n, default_proc)

class MultiMethod(object) :
    def __init__(self, n, default_proc) :
        self.registry_ = {}
        self.n_ = n
        self.default_proc_ = default_proc

    def add_handler(self, f, *types) :
        self.registry_[types] = f

    def register(self, *types) :
        def modifier(f) :
            self.add_handler(f, *types)
            return self
        return modifier

    def __call__(self, *args) :
        types = tuple(type(x) for x in args[:self.n_])
        handler = self.registry_.get(types, self.default_proc_)
        return handler(*args)
