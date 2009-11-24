import os
from clay.error import *
from clay.multimethod import *
from clay.core import *
from clay.ast import *
from clay.parser import parse



#
# ModuleEnvironment
#

_lookupSet = set()

class ModuleEnvironment(object) :
    def __init__(self, module) :
        self.module = module

    def add(self, name, entry) :
        assert type(name) is str
        if name in self.module.globals :
            error("duplicate definition: %s" % name)
        self.module.globals[name] = entry

    def lookup(self, name) :
        entry = self.lookupInternal(name)
        if entry is None :
            error("undefined name: %s" % name)
        return entry

    def lookupInternal(self, name) :
        entry = self.module.globals.get(name)
        if entry is not None :
            return entry
        return self.lookupModuleLinks(name, self.module.imports)

    def lookupExternal(self, name) :
        if self in _lookupSet :
            return None
        _lookupSet.add(self)
        try :
            entry = self.module.globals.get(name)
            if entry is not None :
                return entry
            return self.lookupModuleLinks(name, self.module.exports)
        finally :
            _lookupSet.remove(self)

    def lookupModuleLinks(self, name, links) :
        entry = None
        for link in links :
            entry2 = link.module.env.lookupExternal(name)
            if entry2 is not None :
                if entry is None :
                    entry = entry2
                elif entry is not entry2 :
                    error("name is ambiguous: %s" % name)
        return entry



#
# Environment
#

class Environment(object) :
    def __init__(self, parent=None) :
        self.parent = parent
        self.entries = {}

    def add(self, name, entry) :
        assert type(name) is str
        if name in self.entries :
            error("duplicate definition: %s" % name)
        self.entries[name] = entry

    def lookupInternal(self, name) :
        assert type(name) is str
        entry = self.entries.get(name)
        if (entry is None) and (self.parent is not None) :
            return self.parent.lookupInternal(name)
        return entry

    def lookup(self, name) :
        entry = self.lookupInternal(name)
        if entry is None :
            error("undefined name: %s" % name)
        return entry



#
# addIdent, lookupIdent
#

def addIdent(env, ident, value) :
    withContext(ident, lambda : env.add(ident.s, value))

def lookupIdent(env, ident, converter=(lambda x : x)) :
    return withContext(ident, lambda : converter(env.lookup(ident.s)))



#
# StaticValue
#

class StaticValue(object) :
    def __init__(self, value) :
        self.value = value



#
# primitives module
#

primitivesModule = Module([], [], [])
primitivesEnv = ModuleEnvironment(primitivesModule)
primitivesModule.env = primitivesEnv

def installPrimitive(name, value) :
    primitivesEnv.add(name, value)



#
# setModuleSearchPath, locateModule
#

moduleSearchPath = []

def setModuleSearchPath(searchPath) :
    global moduleSearchPath
    moduleSearchPath = searchPath

def locateModule(names) :
    for d in moduleSearchPath :
        fileName = os.path.join(d, *names) + ".clay"
        if os.path.isfile(fileName) :
            return fileName
    error("module not found: %s" % "".join(names))



#
# loadProgram
#

moduleMap = {}

def loadProgram(fileName) :
    module = loadModuleFile(fileName)
    resolveLinks(module.imports)
    resolveLinks(module.exports)
    initModule1(module)
    initModule2(module)
    return module

def loadModuleFile(fileName) :
    data = file(fileName).read()
    module = parse(data, fileName)
    return module

def resolveLinks(links) :
    for link in links :
        nameParts = [x.s for x in link.dottedName.names]
        link.module = loadModule(nameParts)

def loadModule(nameParts) :
    nameStr = ".".join(nameParts)
    if nameStr == "__primitives__" :
        return primitivesModule
    module = moduleMap.get(nameStr)
    if module is None :
        fileName = locateModule(nameParts)
        module = loadModuleFile(fileName)
        moduleMap[nameStr] = module
        resolveLinks(module.imports)
        resolveLinks(module.exports)
    return module

def loadedModule(nameStr) :
    module = moduleMap.get(nameStr)
    ensure(module is not None, "module not loaded: %s" % nameStr)
    return module



#
# initModule stage 1
#

def initModule1(module) :
    if module.initialized1 :
        return
    module.initialized1 = True
    for import_ in module.imports :
        initModule1(import_.module)
    for export in module.exports :
        initModule1(export.module)
    module.env = ModuleEnvironment(module)
    for item in module.topLevelItems :
        initTopLevelItem1(item, module)

initTopLevelItem1 = multimethod("initTopLevelItem1")

@initTopLevelItem1.register(Record)
def foo(x, module) :
    x.env = module.env
    if len(x.typeVars) == 0 :
        v = recordType(x, [])
    else :
        v = x
    addIdent(module.env, x.name, StaticValue(toCOValue(v)))

@initTopLevelItem1.register(Procedure)
def foo(x, module) :
    x.code.env = module.env
    addIdent(module.env, x.name, StaticValue(toCOValue(x)))

@initTopLevelItem1.register(Overloadable)
def foo(x, module) :
    addIdent(module.env, x.name, StaticValue(toCOValue(x)))

@initTopLevelItem1.register(Overload)
def foo(x, module) :
    x.code.env = module.env

@initTopLevelItem1.register(ExternalProcedure)
def foo(x, module) :
    x.env = module.env
    addIdent(module.env, x.name, StaticValue(toCOValue(x)))



#
# initModule stage 2
#

def initModule2(module) :
    if module.initialized2 :
        return
    module.initialized2 = True
    for import_ in module.imports :
        initModule2(import_.module)
    for export in module.exports :
        initModule2(export.module)
    for item in module.topLevelItems :
        initTopLevelItem2(item, module)

initTopLevelItem2 = multimethod("initTopLevelItem2")

@initTopLevelItem2.register(TopLevelItem)
def foo(x, module) :
    pass

@initTopLevelItem2.register(Overload)
def foo(x, module) :
    y = lookupIdent(module.env, x.name)
    ensure(type(y) is StaticValue, "invalid overloadable")
    z = lower(y.value)
    ensure(type(z) is Overloadable, "invalid overloadable")
    z.overloads.insert(0, x)



#
# extendEnv
#

def extendEnv(parentEnv, variables, objects) :
    env = Environment(parentEnv)
    for variable, object_ in zip(variables, objects) :
        addIdent(env, variable, object_)
    return env



#
# syntactic closures
#

class SCExpression(Expression) :
    def __init__(self, env, expr) :
        super(SCExpression, self).__init__()
        self.env = env
        self.expr = expr

def primitiveNameRef(s) :
    nameRef = NameRef(Identifier(s))
    return SCExpression(primitivesEnv, nameRef)

def coreNameRef(s) :
    nameRef = NameRef(Identifier(s))
    env = loadedModule("_core").env
    return SCExpression(env, nameRef)
