import os
from clay.cleanup import *
from clay.error import *
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
    def __init__(self, parent=None, filter=None) :
        self.parent = parent
        self.entries = {}
        self.filter = filter

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
        if self.filter is not None :
            entry = self.filter(entry)
        return entry



#
# addIdent, lookupIdent
#

def addIdent(env, ident, value) :
    withContext(ident, lambda : env.add(ident.s, value))

def lookupIdent(env, ident, verifier=None) :
    if verifier is None :
        verifier = lambda x : x
    return withContext(ident, lambda : verifier(env.lookup(ident.s)))



#
# install primitives
#

primitivesModule = Module([], [], [])
primitivesEnv = ModuleEnvironment(primitivesModule)
primitivesModule.env = primitivesEnv
primitives = None

def installPrimitive(name, value) :
    primitivesEnv.add(name, value)

def installDefaultPrimitives() :
    primClasses = {}
    def entry(name, value) :
        installPrimitive(name, value)
    def primitive(name) :
        primClass = type("Primitive%s" % name, (object,), {})
        primClasses[name] = primClass
        prim = primClass()
        prim.name = name
        entry(name, prim)
    def overloadable(name) :
        x = Overloadable(Identifier(name))
        x.env = primitivesEnv
        entry(name, x)

    primitive("_print")

    primitive("Tuple")
    primitive("Array")
    primitive("Pointer")

    primitive("default")
    primitive("typeSize")

    primitive("primitiveCopy")

    primitive("addressOf")
    primitive("pointerDereference")
    primitive("pointerToInt")
    primitive("intToPointer")
    primitive("pointerCast")
    primitive("allocateMemory")
    primitive("freeMemory")

    primitive("TupleType")
    primitive("tuple")
    primitive("tupleFieldCount")
    primitive("tupleFieldRef")

    primitive("array")
    primitive("arrayRef")

    primitive("RecordType")
    primitive("recordFieldCount")
    primitive("recordFieldRef")

    primitive("numericEquals")
    primitive("numericLesser")
    primitive("numericAdd")
    primitive("numericSubtract")
    primitive("numericMultiply")
    primitive("numericDivide")
    primitive("numericRemainder")
    primitive("numericNegate")
    primitive("numericConvert")

    primitive("shiftLeft")
    primitive("shiftRight")
    primitive("bitwiseAnd")
    primitive("bitwiseOr")
    primitive("bitwiseXor")

    overloadable("init")
    overloadable("copy")
    overloadable("destroy")
    overloadable("assign")

    overloadable("equals")
    overloadable("notEquals")
    overloadable("lesser")
    overloadable("lesserEquals")
    overloadable("greater")
    overloadable("greaterEquals")

    overloadable("plus")
    overloadable("minus")
    overloadable("add")
    overloadable("subtract")
    overloadable("multiply")
    overloadable("divide")
    overloadable("remainder")

    overloadable("hash")

    overloadable("iterator")
    overloadable("hasNext")
    overloadable("next")

    Primitives = type("Primitives", (object,), primClasses)
    global primitives
    primitives = Primitives()

installDefaultPrimitives()



#
# loadProgram
#

def loadProgram(fileName) :
    module = loadModuleFile(fileName)
    resolveLinks(module.imports)
    resolveLinks(module.exports)
    installOverloads(module)
    return module

def loadModuleFile(fileName) :
    data = file(fileName).read()
    module = parse(data, fileName)
    env = ModuleEnvironment(module)
    module.env = env
    for item in module.topLevelItems :
        item.env = env
        if type(item) is not Overload :
            addIdent(env, item.name, item)
    return module

def resolveLinks(links) :
    for link in links :
        link.module = loadModule(link.dottedName)

def loadModule(dottedName) :
    names = [x.s for x in dottedName.names]
    nameStr = ".".join(names)
    if nameStr == "__primitives__" :
        return primitivesModule
    module = _modules.get(nameStr)
    if module is None :
        fileName = locateModule(names)
        module = loadModuleFile(fileName)
        _modules[nameStr] = module
        resolveLinks(module.imports)
        resolveLinks(module.exports)
    return module

_modules = {}
def _cleanupModules() :
    global _modules
    _modules = {}
installGlobalsCleanupHook(_cleanupModules)



#
# setModuleSearchPath, locateModule
#

_moduleSearchPath = []

def setModuleSearchPath(searchPath) :
    global _moduleSearchPath
    _moduleSearchPath = searchPath

def locateModule(names) :
    for d in _moduleSearchPath :
        fileName = os.path.join(d, *names) + ".clay"
        if os.path.isfile(fileName) :
            return fileName
    error("module not found: %s" % "".join(names))



#
# installOverloads
#

def installOverloads(module) :
    installOverloads2(module, set())

def installOverloads2(module, moduleSet) :
    if module in moduleSet :
        return
    moduleSet.add(module)
    try :
        for import_ in module.imports :
            installOverloads2(import_.module, moduleSet)
        for export in module.exports :
            installOverloads2(export.module, moduleSet)
        for item in module.topLevelItems :
            if type(item) is Overload :
                entry = lookupIdent(module.env, item.name, verifyOverloadable)
                entry.overloads.insert(0, item)
    finally :
        moduleSet.remove(module)

def verifyOverloadable(x) :
    ensure(type(x) is Overloadable, "invalid overloadable")
    return x



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
