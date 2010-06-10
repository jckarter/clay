#include "clay.hpp"
#include <cstdio>
#include <cassert>

using namespace std;

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif


static vector<string> searchPath;
static vector<string> moduleSuffixes;
static map<string, ModulePtr> modules;

static BlockPtr gvarInitializers;
static BlockPtr gvarDestructors;



//
// initModuleSuffixes
//

static const char *getOS() {
#if defined(__APPLE__)
    return "macosx";
#elif defined(linux)
    return "linux";
#elif defined(_WIN32)
    return "windows";
#else
    #error "Unsupported operating system"
#endif
}

static const char *getPtrSize() {
    switch (sizeof(void*)) {
    case 4 : return "32";
    case 8 : return "64";
    default :
        assert(false);
        return NULL;
    }
}

static void initModuleSuffixes() {
    string os = getOS();
    string bits = getPtrSize();
    moduleSuffixes.push_back("." + os + "." + bits + ".clay");
    moduleSuffixes.push_back("." + os + ".clay");
    moduleSuffixes.push_back("." + bits + ".clay");
#ifndef _WIN32
    moduleSuffixes.push_back(".unix.clay");
#endif    
    moduleSuffixes.push_back(".clay");
}



//
// addSearchPath, locateFile, toRelativePath
//

void addSearchPath(const string &path) {
    searchPath.push_back(path);
}

static bool locateFile(const string &relativePath, string &path) {
    // relativePath has no suffix
    if (moduleSuffixes.empty())
        initModuleSuffixes();
    string tail = PATH_SEPARATOR + relativePath;
    for (unsigned i = 0; i < searchPath.size(); ++i) {
        string pathWOSuffix = searchPath[i] + tail;
        for (unsigned j = 0; j < moduleSuffixes.size(); ++j) {
            string p = pathWOSuffix + moduleSuffixes[j];
            FILE *f = fopen(p.c_str(), "rb");
            if (f != NULL) {
                fclose(f);
                path = p;
                return true;
            }
        }
    }
    return false;
}

static string toRelativePath(DottedNamePtr name) {
    string relativePath;
    for (unsigned i = 0; i < name->parts.size(); ++i) {
        relativePath.append(name->parts[i]->str);
        relativePath.push_back(PATH_SEPARATOR);
    }
    relativePath.append(name->parts.back()->str);
    // relative path has no suffix
    return relativePath;
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

static string toKey(DottedNamePtr name) {
    string key;
    for (unsigned i = 0; i < name->parts.size(); ++i) {
        if (i != 0)
            key.push_back('.');
        key.append(name->parts[i]->str);
    }
    return key;
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

static ModulePtr loadPrelude() {
    DottedNamePtr dottedName = new DottedName();
    dottedName->parts.push_back(new Identifier("prelude"));
    return loadModuleByName(dottedName);
}

ModulePtr loadProgram(const string &fileName) {
    ModulePtr m = parse(loadFile(fileName));
    ModulePtr prelude = loadPrelude();
    loadDependents(m);
    installGlobals(m);
    gvarInitializers = new Block();
    gvarDestructors = new Block();
    initModule(prelude);
    initModule(m);
    return m;
}

BlockPtr globalVarInitializers() {
    return gvarInitializers;
}

BlockPtr globalVarDestructors() {
    return gvarDestructors;
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
    case PROCEDURE : {
        Procedure *z = (Procedure *)y.ptr();
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
            StatementPtr z = new ForeignStatement(x->env, y);
            z->location = y->location;
            gvarInitializers->statements.push_back(z);

            ExprPtr destructor = kernelNameRef("destroy");
            CallPtr destroyCall = new Call(destructor);
            destroyCall->location = lhs->location;
            destroyCall->args.push_back(new ForeignExpr(x->env, lhs));
            StatementPtr a = new ExprStatement(destroyCall.ptr());
            gvarDestructors->statements.insert(
                gvarDestructors->statements.begin(), a);
            break;
        }
        }
    }
}



//
// prim op names
//

static map<int, string> primOpNames;

const string &primOpName(PrimOpPtr x) {
    map<int, string>::iterator i = primOpNames.find(x->primOpCode);
    assert(i != primOpNames.end());
    return i->second;
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

static void addPrimOp(ModulePtr m, const string &name, PrimOpPtr x) {
    primOpNames[x->primOpCode] = name;
    addPrim(m, name, x.ptr());
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

#define PRIMITIVE(x) addPrimOp(prims, toPrimStr(#x), new PrimOp(PRIM_##x))

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
    PRIMITIVE(pointerEqualsP);
    PRIMITIVE(pointerLesserP);
    PRIMITIVE(pointerOffset);
    PRIMITIVE(pointerToInt);
    PRIMITIVE(intToPointer);

    PRIMITIVE(CodePointerP);
    PRIMITIVE(CodePointer);
    PRIMITIVE(RefCodePointer);
    PRIMITIVE(makeCodePointer);

    PRIMITIVE(CCodePointerP);
    PRIMITIVE(CCodePointer);
    PRIMITIVE(StdCallCodePointer);
    PRIMITIVE(FastCallCodePointer);
    PRIMITIVE(makeCCodePointer);

    PRIMITIVE(pointerCast);

    PRIMITIVE(Array);
    PRIMITIVE(array);
    PRIMITIVE(arrayRef);

    PRIMITIVE(TupleP);
    PRIMITIVE(Tuple);
    PRIMITIVE(TupleElementCount);
    PRIMITIVE(TupleElementOffset);
    PRIMITIVE(tuple);
    PRIMITIVE(tupleRef);

    PRIMITIVE(RecordP);
    PRIMITIVE(RecordFieldCount);
    PRIMITIVE(RecordFieldOffset);
    PRIMITIVE(RecordFieldIndex);
    PRIMITIVE(recordFieldRef);
    PRIMITIVE(recordFieldRefByName);

    PRIMITIVE(Static);
    PRIMITIVE(StaticName);

    PRIMITIVE(EnumP);
    PRIMITIVE(enumToInt);
    PRIMITIVE(intToEnum);

    PRIMITIVE(throw);

#undef PRIMITIVE

    return prims;
}



//
// access names from other modules
//

ObjectPtr kernelName(const string &name) {
    return lookupPublic(loadedModule("prelude"), new Identifier(name));
}

ObjectPtr primName(const string &name) {
    return lookupPublic(loadedModule("__primitives__"), new Identifier(name));
}

ExprPtr moduleNameRef(const string &module, const string &name) {
    ExprPtr nameRef = new NameRef(new Identifier(name));
    return new ForeignExpr(loadedModule(module)->env, nameRef);
}

ExprPtr kernelNameRef(const string &name) {
    return moduleNameRef("prelude", name);
}

ExprPtr primNameRef(const string &name) {
    return moduleNameRef("__primitives__", name);
}



//
// ForeignExpr::getEnv, ForeignStatement::getEnv
//

EnvPtr ForeignExpr::getEnv() {
    if (!foreignEnv) {
        assert(moduleName.size() > 0);
        foreignEnv = loadedModule(moduleName)->env;
    }
    return foreignEnv;
}

EnvPtr ForeignStatement::getEnv() {
    if (!foreignEnv) {
        assert(moduleName.size() > 0);
        foreignEnv = loadedModule(moduleName)->env;
    }
    return foreignEnv;
}
