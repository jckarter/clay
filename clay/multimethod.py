__all__ = ["multimethod"]

class DummyDefault(object) : pass

def multimethod(n=1, default=DummyDefault) :
    return MultiMethod(n, default)

class MultiMethod(object) :
    def __init__(self, n, default) :
        self.registry_ = {}
        self.n_ = n
        self.default_ = default

    def add_handler(self, f, *types) :
        self.registry_[types] = f

    def register(self, *types) :
        def modifier(f) :
            self.add_handler(f, *types)
            return self
        return modifier

    def __call__(self, *args) :
        types = tuple(type(x) for x in args[:self.n_])
        handler = self.registry_.get(types)
        if handler is None :
            assert self.default_ is not DummyDefault
            return self.default_
        return handler(*args)
