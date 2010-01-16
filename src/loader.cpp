#include "clay.hpp"
#include <cstdio>
#include <cassert>

using namespace std;

#ifdef WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

static vector<string> searchPath;
static map<string, ModulePtr> modules;



//
// addSearchPath, locateFile
//

void addSearchPath(const string &path) {
    searchPath.push_back(path);
}

static bool locateFile(const string &relativePath, string &path) {
    string tail = PATH_SEPARATOR + relativePath;
    vector<string>::iterator i, end;
    for (i = searchPath.begin(), end = searchPath.end(); i != end; ++i) {
        string p = (*i) + tail;
        FILE *f = fopen(p.c_str(), "rb");
        if (f != NULL) {
            fclose(f);
            path = p;
            return true;
        }
    }
    return false;
}



//
// toRelativePath, toKey
//

static string toRelativePath(DottedNamePtr name) {
    string relativePath;
    vector<IdentifierPtr>::iterator i, end;
    bool first = true;
    for (i = name->parts.begin(), end = name->parts.end(); i != end; ++i) {
        if (!first)
            relativePath.push_back(PATH_SEPARATOR);
        else
            first = false;
        relativePath.append((*i)->str);
    }
    relativePath.append(".clay");
    return relativePath;
}

static string toKey(DottedNamePtr name) {
    string key;
    vector<IdentifierPtr>::iterator i, end;
    bool first = true;
    for (i = name->parts.begin(), end = name->parts.end(); i != end; ++i) {
        if (!first)
            key.push_back('.');
        else
            first = false;
        key.append((*i)->str);
    }
    return key;
}



//
// loadFile
//

static SourcePtr loadFile(const string &fileName) {
    FILE *f = fopen(fileName.c_str(), "rb");
    if (!f)
        error("unable to open file: " + fileName);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    assert(size >= 0);
    char *buf = new char [size + 1];
    long n = fread(buf, 1, size, f);
    fclose(f);
    assert(n == size);
    buf[size] = 0;
    return new Source(fileName, buf, size);
}



//
// loadModuleByName, loadDependents, loadProgram
//

static void loadDependents(ModulePtr m);
static void initializeModule(ModulePtr m);
static ModulePtr makePrimitivesModule();

static void installGlobals(ModulePtr m) {
    m->env = new Env(m);
    vector<TopLevelItemPtr>::iterator i, end;
    for (i = m->topLevelItems.begin(), end = m->topLevelItems.end();
         i != end; ++i) {
        TopLevelItem *x = i->ptr();
        x->env = m->env;
        switch (x->objKind) {
        case RECORD : {
            Record *y = (Record *)x;
            ObjectPtr value;
            if (y->patternVars.empty())
                value = recordType(y, vector<ValuePtr>()).ptr();
            else
                value = y;
            addGlobal(m, ((Record *)x)->name, value);
            break;
        }
        case PROCEDURE :
            addGlobal(m, ((Procedure *)x)->name, x);
            break;
        case OVERLOADABLE :
            addGlobal(m, ((Overloadable *)x)->name, x);
            break;
        case EXTERNAL_PROCEDURE :
            addGlobal(m, ((ExternalProcedure *)x)->name, x);
            break;
        case OVERLOAD :
            break;
        default :
            assert(false);
        }
    }
}

static ModulePtr loadModuleByName(DottedNamePtr name) {
    string key = toKey(name);

    map<string, ModulePtr>::iterator i = modules.find(key);
    if (i != modules.end())
        return i->second;

    ModulePtr module;

    if (key == "__primitives__") {
        module = makePrimitivesModule();
    }
    else {
        string relativePath = toRelativePath(name);
        string path;
        if (!locateFile(relativePath, path))
            error(name, "module not found: " + key);
        module = parse(loadFile(path));
    }

    modules[key] = module;
    loadDependents(module);
    installGlobals(module);

    return module;
}

static void loadDependents(ModulePtr m) {
    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii)
        (*ii)->module = loadModuleByName((*ii)->dottedName);
    vector<ExportPtr>::iterator ei, eend;
    for (ei = m->exports.begin(), eend = m->exports.end(); ei != eend; ++ei)
        (*ei)->module = loadModuleByName((*ei)->dottedName);
}

ModulePtr loadProgram(const string &fileName) {
    ModulePtr m = parse(loadFile(fileName));
    loadDependents(m);
    installGlobals(m);
    initializeModule(m);
    return m;
}

ModulePtr loadedModule(const string &module) {
    if (!modules.count(module))
        error("module not loaded: " + module);
    return modules[module];
}



//
// initializeModule
//

static void initializeModule(ModulePtr m) {
    if (m->initialized) return;
    m->initialized = true;
    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii)
        initializeModule((*ii)->module);
    vector<ExportPtr>::iterator ei, eend;
    for (ei = m->exports.begin(), eend = m->exports.end(); ei != eend; ++ei)
        initializeModule((*ei)->module);

    vector<TopLevelItemPtr>::iterator ti, tend;
    for (ti = m->topLevelItems.begin(), tend = m->topLevelItems.end();
         ti != tend; ++ti) {
        Object *obj = ti->ptr();
        if (obj->objKind == OVERLOAD) {
            Overload *x = (Overload *)obj;
            ObjectPtr y = lookupGlobal(m, x->name);
            if (y->objKind != OVERLOADABLE)
                error(x->name, "invalid overloadable");
            Overloadable *z = (Overloadable *)y.ptr();
            z->overloads.insert(z->overloads.begin(), x);
        }
    }
}



//
// makePrimitivesModule
//

static string toPrimStr(const string &s) {
    string name = s;
    if (name[s.size() - 1] == 'P')
        name[s.size() - 1] = '?';
    return name;
}

static ModulePtr makePrimitivesModule() {
    ModulePtr prims = new Module();

    prims->globals["Bool"] = boolType.ptr();
    prims->globals["Int8"] = int8Type.ptr();
    prims->globals["Int16"] = int16Type.ptr();
    prims->globals["Int32"] = int32Type.ptr();
    prims->globals["Int64"] = int64Type.ptr();
    prims->globals["UInt8"] = uint8Type.ptr();
    prims->globals["UInt16"] = uint16Type.ptr();
    prims->globals["UInt32"] = uint32Type.ptr();
    prims->globals["UInt64"] = uint64Type.ptr();
    prims->globals["Float32"] = float32Type.ptr();
    prims->globals["Float64"] = float64Type.ptr();
    prims->globals["CompilerObject"] = compilerObjectType.ptr();
    prims->globals["Void"] = voidType.ptr();

#define PRIMITIVE(x) prims->globals[toPrimStr(#x)] = new PrimOp(PRIM_##x)
    PRIMITIVE(TypeP);
    PRIMITIVE(TypeSize);

    PRIMITIVE(primitiveInit);
    PRIMITIVE(primitiveDestroy);
    PRIMITIVE(primitiveCopy);
    PRIMITIVE(primitiveAssign);
    PRIMITIVE(primitiveEqualsP);
    PRIMITIVE(primitiveHash);

    PRIMITIVE(boolNot);
    PRIMITIVE(boolTruth);

    PRIMITIVE(numericEqualsP);
    PRIMITIVE(numericLesserP);
    PRIMITIVE(numericAdd);
    PRIMITIVE(numericSubtract);
    PRIMITIVE(numericMultiply);
    PRIMITIVE(numericDivide);
    PRIMITIVE(numericNegate);

    PRIMITIVE(integerRemainder);
    PRIMITIVE(integerShiftLeft);
    PRIMITIVE(integerShiftRight);
    PRIMITIVE(integerBitwiseAnd);
    PRIMITIVE(integerBitwiseOr);
    PRIMITIVE(integerBitwiseXor);

    PRIMITIVE(numericConvert);

    PRIMITIVE(Pointer);

    PRIMITIVE(addressOf);
    PRIMITIVE(pointerDereference);
    PRIMITIVE(pointerToInt);
    PRIMITIVE(intToPointer);
    PRIMITIVE(pointerCast);
    PRIMITIVE(allocateMemory);
    PRIMITIVE(freeMemory);

    PRIMITIVE(Array);
    PRIMITIVE(array);
    PRIMITIVE(arrayRef);

    PRIMITIVE(TupleTypeP);
    PRIMITIVE(Tuple);
    PRIMITIVE(TupleElementCount);
    PRIMITIVE(TupleElementType);
    PRIMITIVE(TupleElementOffset);
    PRIMITIVE(tuple);
    PRIMITIVE(tupleRef);

    PRIMITIVE(RecordTypeP);
    PRIMITIVE(RecordFieldCount);
    PRIMITIVE(RecordFieldType);
    PRIMITIVE(RecordFieldOffset);
    PRIMITIVE(RecordFieldIndex);
    PRIMITIVE(recordFieldRef);
    PRIMITIVE(recordFieldRefByName);
    PRIMITIVE(recordInit);
    PRIMITIVE(recordDestroy);
    PRIMITIVE(recordCopy);
    PRIMITIVE(recordAssign);
    PRIMITIVE(recordEqualsP);
    PRIMITIVE(recordHash);
#undef PRIMITIVE
    return prims;
}



//
// access names from other modules
//

ObjectPtr coreName(const string &name) {
    return lookupPublic(loadedModule("_core"), new Identifier(name));
}

ObjectPtr primName(const string &name) {
    return lookupPublic(loadedModule("__primitives__"), new Identifier(name));
}

ExprPtr moduleNameRef(const string &module, const string &name) {
    ExprPtr nameRef = new NameRef(new Identifier(name));
    return new SCExpr(loadedModule(module)->env, nameRef);
}

ExprPtr coreNameRef(const string &name) {
    return moduleNameRef("_core", name);
}

ExprPtr primNameRef(const string &name) {
    return moduleNameRef("__primitives__", name);
}
