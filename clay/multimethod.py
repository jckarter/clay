__all__ = ["multimethod"]

def multimethod(n=1) :
    return MultiMethod(n)

class MultiMethod(object) :
    def __init__(self, n) :
        self.registry_ = {}
        self.n_ = n

    def add_handler(self, f, *types) :
        self.registry_[types] = f

    def register(self, *types) :
        def modifier(f) :
            self.add_handler(f, *types)
            return self
        return modifier

    def __call__(self, *args) :
        types = tuple(type(x) for x in args[:self.n_])
        handler = self.registry_[types]
        return handler(*args)
