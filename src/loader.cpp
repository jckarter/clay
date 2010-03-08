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

static BlockPtr gvarInitializers;;



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
    for (unsigned i = 0; i < name->parts.size(); ++i) {
        relativePath.append(name->parts[i]->str);
        relativePath.push_back(PATH_SEPARATOR);
    }
    relativePath.append(name->parts.back()->str);
    relativePath.append(".clay");
    return relativePath;
}

static string toKey(DottedNamePtr name) {
    string key;
    for (unsigned i = 0; i < name->parts.size(); ++i) {
        if (i != 0)
            key.push_back('.');
        key.append(name->parts[i]->str);
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
static void initModule(ModulePtr m);
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
            Object *z = y;
            if (y->patternVars.empty()) {
                TypePtr t = recordType(y, vector<ObjectPtr>());
                z = t.ptr();
            }
            addGlobal(m, y->name, y->visibility, z);
            break;
        }
        case ENUMERATION : {
            Enumeration *y = (Enumeration *)x;
            TypePtr t = enumType(y);
            addGlobal(m, y->name, y->visibility, t.ptr());
            for (unsigned i = 0 ; i < y->members.size(); ++i) {
                EnumMember *z = y->members[i].ptr();
                z->index = (int)i;
                z->type = t;
                addGlobal(m, z->name, y->visibility, z);
            }
            break;
        }
        default :
            if (x->name.ptr())
                addGlobal(m, x->name, x->visibility, x);
            break;
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

static ModuleHolderPtr installHolder(ModuleHolderPtr mh, IdentifierPtr name) {
    ModuleHolderPtr holder;
    map<string, ModuleHolderPtr>::iterator i = mh->children.find(name->str);
    if (i != mh->children.end())
        return i->second;
    holder = new ModuleHolder();
    mh->children[name->str] = holder;
    return holder;
}

static void loadDependents(ModulePtr m) {
    m->rootHolder = new ModuleHolder();
    m->publicRootHolder = new ModuleHolder();
    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii) {
        ImportPtr x = *ii;
        x->module = loadModuleByName(x->dottedName);
        switch (x->importKind) {
        case IMPORT_MODULE : {
            ImportModule *y = (ImportModule *)x.ptr();
            {
                ModuleHolderPtr holder = m->rootHolder;
                if (y->alias.ptr()) {
                    holder = installHolder(holder, y->alias);
                }
                else {
                    vector<IdentifierPtr> &parts = y->dottedName->parts;
                    for (unsigned i = 0; i < parts.size(); ++i)
                        holder = installHolder(holder, parts[i]);
                }
                if (holder->import.ptr())
                    error(x, "module already imported");
                holder->import = y;
            }
            if (y->visibility == PUBLIC) {
                ModuleHolderPtr holder = m->publicRootHolder;
                if (y->alias.ptr()) {
                    holder = installHolder(holder, y->alias);
                }
                else {
                    vector<IdentifierPtr> &parts = y->dottedName->parts;
                    for (unsigned i = 0; i < parts.size(); ++i)
                        holder = installHolder(holder, parts[i]);
                }
                if (holder->import.ptr())
                    error(x, "module already imported");
                holder->import = y;
            }
            break;
        }
        case IMPORT_STAR : {
            break;
        }
        case IMPORT_MEMBERS : {
            ImportMembers *y = (ImportMembers *)x.ptr();
            for (unsigned i = 0; i < y->members.size(); ++i) {
                ImportedMember &z = y->members[i];
                IdentifierPtr alias = z.alias;
                if (!alias)
                    alias = z.name;
                if (y->aliasMap.count(alias->str))
                    error(alias, "name imported already: " + alias->str);
                y->aliasMap[alias->str] = z.name;
            }
            break;
        }
        default :
            assert(false);
        }
    }
}

ModulePtr loadProgram(const string &fileName) {
    ModulePtr m = parse(loadFile(fileName));
    loadDependents(m);
    installGlobals(m);
    gvarInitializers = new Block();
    initModule(m);
    return m;
}

BlockPtr globalVarInitializers() {
    return gvarInitializers;
}

ModulePtr loadedModule(const string &module) {
    if (!modules.count(module))
        error("module not loaded: " + module);
    return modules[module];
}



//
// initOverload, initStaticOverload, initModule
//

static void initOverload(OverloadPtr x) {
    EnvPtr env = new Env(x->env);
    for (unsigned i = 0; i < x->code->patternVars.size(); ++i) {
        PatternCellPtr cell = new PatternCell(x->code->patternVars[i], NULL);
        addLocal(env, x->code->patternVars[i], cell.ptr());
    }
    PatternPtr pattern = evaluatePattern(x->target, env);
    ObjectPtr y = reducePattern(pattern);
    switch (y->objKind) {
    case OVERLOADABLE : {
        Overloadable *z = (Overloadable *)y.ptr();
        z->overloads.insert(z->overloads.begin(), x);
        break;
    }
    case RECORD : {
        Record *z = (Record *)y.ptr();
        z->overloads.insert(z->overloads.begin(), x);
        break;
    }
    case TYPE :
    case PATTERN : {
        addTypeOverload(x);
        break;
    }
    default : {
        error(x->target, "invalid overload target");
    }
    }
}

static void initStaticOverload(StaticOverloadPtr x) {
    ObjectPtr y = evaluateStatic(x->target, x->env);
    if (y->objKind != STATIC_OVERLOADABLE)
        error(x->target, "invalid static overload target");
    StaticOverloadable *z = (StaticOverloadable *)y.ptr();
    z->overloads.insert(z->overloads.begin(), x);
}

static void initModule(ModulePtr m) {
    if (m->initialized) return;
    m->initialized = true;
    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii)
        initModule((*ii)->module);

    const vector<TopLevelItemPtr> &items = m->topLevelItems;
    vector<TopLevelItemPtr>::const_iterator ti, tend;
    for (ti = items.begin(), tend = items.end(); ti != tend; ++ti) {
        Object *obj = ti->ptr();
        switch (obj->objKind) {
        case OVERLOAD :
            initOverload((Overload *)obj);
            break;
        case GLOBAL_VARIABLE : {
            GlobalVariable *x = (GlobalVariable *)obj;
            ExprPtr lhs = new NameRef(x->name);
            lhs->location = x->name->location;
            StatementPtr y = new InitAssignment(lhs, x->expr);
            y->location = x->location;
            StatementPtr z = new SCStatement(x->env, y);
            z->location = y->location;
            gvarInitializers->statements.push_back(z);
            break;
        }
        case STATIC_OVERLOAD :
            initStaticOverload((StaticOverload *)obj);
            break;
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

static void addPrim(ModulePtr m, const string &name, ObjectPtr x) {
    m->globals[name] = x;
    m->publicGlobals[name] = x;
}

static ModulePtr makePrimitivesModule() {
    ModulePtr prims = new Module();

    addPrim(prims, "Bool", boolType.ptr());
    addPrim(prims, "Int8", int8Type.ptr());
    addPrim(prims, "Int16", int16Type.ptr());
    addPrim(prims, "Int32", int32Type.ptr());
    addPrim(prims, "Int64", int64Type.ptr());
    addPrim(prims, "UInt8", uint8Type.ptr());
    addPrim(prims, "UInt16", uint16Type.ptr());
    addPrim(prims, "UInt32", uint32Type.ptr());
    addPrim(prims, "UInt64", uint64Type.ptr());
    addPrim(prims, "Float32", float32Type.ptr());
    addPrim(prims, "Float64", float64Type.ptr());
    addPrim(prims, "Void", voidType.ptr());

#define PRIMITIVE(x) addPrim(prims, toPrimStr(#x), new PrimOp(PRIM_##x))

    PRIMITIVE(TypeOf);

    PRIMITIVE(TypeP);
    PRIMITIVE(TypeSize);

    PRIMITIVE(primitiveCopy);

    PRIMITIVE(boolNot);

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
    PRIMITIVE(integerBitwiseNot);

    PRIMITIVE(numericConvert);

    PRIMITIVE(Pointer);

    PRIMITIVE(addressOf);
    PRIMITIVE(pointerDereference);
    PRIMITIVE(pointerToInt);
    PRIMITIVE(intToPointer);

    PRIMITIVE(CodePointerTypeP);
    PRIMITIVE(CodePointer);
    PRIMITIVE(RefCodePointer);
    PRIMITIVE(makeCodePointer);

    PRIMITIVE(CCodePointerTypeP);
    PRIMITIVE(CCodePointer);
    PRIMITIVE(makeCCodePointer);

    PRIMITIVE(pointerCast);

    PRIMITIVE(Array);
    PRIMITIVE(array);
    PRIMITIVE(arrayRef);

    PRIMITIVE(TupleTypeP);
    PRIMITIVE(Tuple);
    PRIMITIVE(TupleElementCount);
    PRIMITIVE(TupleElementOffset);
    PRIMITIVE(tuple);
    PRIMITIVE(tupleRef);

    PRIMITIVE(RecordTypeP);
    PRIMITIVE(RecordFieldCount);
    PRIMITIVE(RecordFieldOffset);
    PRIMITIVE(RecordFieldIndex);
    PRIMITIVE(recordFieldRef);
    PRIMITIVE(recordFieldRefByName);

    PRIMITIVE(StaticObject);

    PRIMITIVE(EnumTypeP);
    PRIMITIVE(enumToInt);
    PRIMITIVE(intToEnum);

#undef PRIMITIVE

    return prims;
}



//
// access names from other modules
//

ObjectPtr kernelName(const string &name) {
    return lookupPublic(loadedModule("kernel"), new Identifier(name));
}

ObjectPtr primName(const string &name) {
    return lookupPublic(loadedModule("__primitives__"), new Identifier(name));
}

ExprPtr moduleNameRef(const string &module, const string &name) {
    ExprPtr nameRef = new NameRef(new Identifier(name));
    return new SCExpr(loadedModule(module)->env, nameRef);
}

ExprPtr kernelNameRef(const string &name) {
    return moduleNameRef("kernel", name);
}

ExprPtr primNameRef(const string &name) {
    return moduleNameRef("__primitives__", name);
}
