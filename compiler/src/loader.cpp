#include "clay.hpp"
#include "libclaynames.hpp"
#include "llvm/ADT/Triple.h"
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

static std::string getOS(llvm::Triple const &triple) {
    switch (triple.getOS()) {
    case llvm::Triple::Darwin : return "macosx";
    case llvm::Triple::DragonFly : return "dragonfly";
    case llvm::Triple::FreeBSD : return "freebsd";
    case llvm::Triple::Linux : return "linux";
    case llvm::Triple::Cygwin :
    case llvm::Triple::MinGW32 :
    case llvm::Triple::MinGW64 :
    case llvm::Triple::Win32 : return "windows";
    case llvm::Triple::NetBSD : return "netbsd";
    case llvm::Triple::OpenBSD : return "openbsd";
    case llvm::Triple::Solaris : return "solaris";
    case llvm::Triple::Haiku : return "haiku";
    case llvm::Triple::Minix : return "minix";
    default : return triple.getOSName().str();
    }
}

static std::string getOSGroup(llvm::Triple const &triple) {
    switch (triple.getOS()) {
    case llvm::Triple::Darwin :
    case llvm::Triple::DragonFly :
    case llvm::Triple::FreeBSD :
    case llvm::Triple::Linux :
    case llvm::Triple::NetBSD :
    case llvm::Triple::OpenBSD :
    case llvm::Triple::Solaris :
    case llvm::Triple::Haiku :
    case llvm::Triple::Minix : return "unix";
    default : return "";
    }
}

static std::string getCPU(llvm::Triple const &triple) {
    switch (triple.getArch()) {
    case llvm::Triple::arm :
    case llvm::Triple::thumb : return "arm";
    case llvm::Triple::ppc :
    case llvm::Triple::ppc64 : return "ppc";
    case llvm::Triple::sparc :
    case llvm::Triple::sparcv9 : return "sparc";
    case llvm::Triple::x86 :
    case llvm::Triple::x86_64 : return "x86";
    default : return triple.getArchName().str();
    }
}

static std::string getPtrSize(const llvm::TargetData *targetData) {
    switch (targetData->getPointerSizeInBits()) {
    case 32 : return "32";
    case 64 : return "64";
    default : assert(false);
    }
}

static void initModuleSuffixes() {
    llvm::Triple triple(llvmModule->getTargetTriple());

    string os = getOS(triple);
    string osgroup = getOSGroup(triple);
    string cpu = getCPU(triple);
    string bits = getPtrSize(llvmTargetData);
    moduleSuffixes.push_back("." + os + "." + cpu + "." + bits + ".clay");
    moduleSuffixes.push_back("." + os + "." + cpu + ".clay");
    moduleSuffixes.push_back("." + os + "." + bits + ".clay");
    moduleSuffixes.push_back("." + cpu + "." + bits + ".clay");
    moduleSuffixes.push_back("." + os + ".clay");
    moduleSuffixes.push_back("." + cpu + ".clay");
    moduleSuffixes.push_back("." + bits + ".clay");
    if (!osgroup.empty())
        moduleSuffixes.push_back("." + osgroup + ".clay");
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
            ObjectPtr z = y;
            if (y->params.empty() && !y->varParam) {
                TypePtr t = recordType(y, vector<ObjectPtr>());
                z = t.ptr();
            }
            addGlobal(m, y->name, y->visibility, z.ptr());
            break;
        }
        case VARIANT : {
            Variant *y = (Variant *)x;
            ObjectPtr z = y;
            if (y->params.empty() && !y->varParam) {
                TypePtr t = variantType(y, vector<ObjectPtr>());
                z = t.ptr();
            }
            addGlobal(m, y->name, y->visibility, z.ptr());
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
        module = parse(key, loadFile(path));
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
    ModulePtr m = parse("__main__", loadFile(fileName));
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
// initOverload, initVariantInstance, initModule
//

static void initOverload(OverloadPtr x) {
    EnvPtr env = new Env(x->env);
    const vector<PatternVar> &pvars = x->code->patternVars;
    for (unsigned i = 0; i < pvars.size(); ++i) {
        if (pvars[i].isMulti) {
            MultiPatternCellPtr cell = new MultiPatternCell(NULL);
            addLocal(env, pvars[i].name, cell.ptr());
        }
        else {
            PatternCellPtr cell = new PatternCell(NULL);
            addLocal(env, pvars[i].name, cell.ptr());
        }
    }
    PatternPtr pattern = evaluateOnePattern(x->target, env);
    ObjectPtr y = derefDeep(pattern);
    if (!y) {
        addTypeOverload(x);
    }
    else {
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
        case VARIANT : {
            Variant *z = (Variant *)y.ptr();
            z->overloads.insert(z->overloads.begin(), x);
            break;
        }
        case TYPE : {
            addTypeOverload(x);
            break;
        }
        case PRIM_OP : {
            if (isOverloadablePrimOp(y))
                addPrimOpOverload((PrimOp *)y.ptr(), x);
            else
                error(x->target, "invalid overload target");
            break;
        }
        default : {
            error(x->target, "invalid overload target");
        }
        }
    }
}

static void initVariantInstance(InstancePtr x) {
    EnvPtr env = new Env(x->env);
    const vector<PatternVar> &pvars = x->patternVars;
    for (unsigned i = 0; i < pvars.size(); ++i) {
        if (pvars[i].isMulti) {
            MultiPatternCellPtr cell = new MultiPatternCell(NULL);
            addLocal(env, pvars[i].name, cell.ptr());
        }
        else {
            PatternCellPtr cell = new PatternCell(NULL);
            addLocal(env, pvars[i].name, cell.ptr());
        }
    }
    PatternPtr pattern = evaluateOnePattern(x->target, env);
    ObjectPtr y = derefDeep(pattern);
    if (y.ptr()) {
        if (y->objKind != TYPE)
            error(x->target, "not a variant type");
        Type *z = (Type *)y.ptr();
        if (z->typeKind != VARIANT_TYPE)
            error(x->target, "not a variant type");
        VariantType *vt = (VariantType *)z;
        vt->variant->instances.push_back(x);
    }
    else {
        if (pattern->kind != PATTERN_STRUCT)
            error(x->target, "not a variant type");
        PatternStruct *ps = (PatternStruct *)pattern.ptr();
        if ((!ps->head) || (ps->head->objKind != VARIANT))
            error(x->target, "not a variant type");
        Variant *v = (Variant *)ps->head.ptr();
        v->instances.push_back(x);
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
        case INSTANCE :
            initVariantInstance((Instance *)obj);
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

            ExprPtr destructor = prelude_expr_destroy();
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
    m->allSymbols[name].insert(x);
    m->publicGlobals[name] = x;
    m->publicSymbols[name].insert(x);
}

static void addPrimOp(ModulePtr m, const string &name, PrimOpPtr x) {
    primOpNames[x->primOpCode] = name;
    addPrim(m, name, x.ptr());
}

static ModulePtr makePrimitivesModule() {
    ModulePtr prims = new Module("__primitives__");

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
    PRIMITIVE(TypeAlignment);
    PRIMITIVE(CallDefinedP);

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

    PRIMITIVE(CodePointer);
    PRIMITIVE(makeCodePointer);

    PRIMITIVE(AttributeStdCall);
    PRIMITIVE(AttributeFastCall);
    PRIMITIVE(AttributeCCall);
    PRIMITIVE(AttributeDLLImport);
    PRIMITIVE(AttributeDLLExport);

    PRIMITIVE(CCodePointerP);
    PRIMITIVE(CCodePointer);
    PRIMITIVE(VarArgsCCodePointer);
    PRIMITIVE(StdCallCodePointer);
    PRIMITIVE(FastCallCodePointer);
    PRIMITIVE(makeCCodePointer);

    PRIMITIVE(pointerCast);

    PRIMITIVE(Array);
    PRIMITIVE(arrayRef);

    PRIMITIVE(Vec);

    PRIMITIVE(Tuple);
    PRIMITIVE(TupleElementCount);
    PRIMITIVE(tupleRef);
    PRIMITIVE(tupleElements);

    PRIMITIVE(Union);
    PRIMITIVE(UnionMemberCount);

    PRIMITIVE(RecordP);
    PRIMITIVE(RecordFieldCount);
    PRIMITIVE(RecordFieldName);
    PRIMITIVE(recordFieldRef);
    PRIMITIVE(recordFieldRefByName);
    PRIMITIVE(recordFields);

    PRIMITIVE(VariantP);
    PRIMITIVE(VariantMemberIndex);
    PRIMITIVE(VariantMemberCount);
    PRIMITIVE(variantRepr);

    PRIMITIVE(Static);
    PRIMITIVE(StaticName);
    PRIMITIVE(staticIntegers);

    PRIMITIVE(EnumP);
    PRIMITIVE(enumToInt);
    PRIMITIVE(intToEnum);

    PRIMITIVE(IdentifierSize);
    PRIMITIVE(IdentifierConcat);
    PRIMITIVE(IdentifierSlice);

#undef PRIMITIVE

    return prims;
}



//
// access names from other modules
//

ModulePtr primitivesModule() {
    static ModulePtr cached;
    if (!cached)
        cached = loadedModule("__primitives__");
    return cached;
}

ModulePtr preludeModule() {
    static ModulePtr cached;
    if (!cached)
        cached = loadedModule("prelude");
    return cached;
}


static IdentifierPtr fnameToIdent(const string &str) {
    string s = str;
    if ((s.size() > 0) && (s[s.size()-1] == 'P'))
        s[s.size()-1] = '?';
    return new Identifier(s);
}

#define DEFINE_PRIMITIVE_ACCESSOR(name) \
    ObjectPtr primitive_##name() { \
        static ObjectPtr cached; \
        if (!cached) \
            cached = safeLookupPublic(primitivesModule(), fnameToIdent(#name)); \
        return cached; \
    } \
    \
    ExprPtr primitive_expr_##name() { \
        static ExprPtr cached; \
        if (!cached) \
            cached = new ObjectExpr(primitive_##name()); \
        return cached; \
    }

DEFINE_PRIMITIVE_ACCESSOR(addressOf);
DEFINE_PRIMITIVE_ACCESSOR(boolNot);
DEFINE_PRIMITIVE_ACCESSOR(Pointer);
DEFINE_PRIMITIVE_ACCESSOR(CodePointer);
DEFINE_PRIMITIVE_ACCESSOR(CCodePointer);
DEFINE_PRIMITIVE_ACCESSOR(VarArgsCCodePointer);
DEFINE_PRIMITIVE_ACCESSOR(StdCallCodePointer);
DEFINE_PRIMITIVE_ACCESSOR(FastCallCodePointer);
DEFINE_PRIMITIVE_ACCESSOR(Array);
DEFINE_PRIMITIVE_ACCESSOR(Vec);
DEFINE_PRIMITIVE_ACCESSOR(Tuple);
DEFINE_PRIMITIVE_ACCESSOR(Union);
DEFINE_PRIMITIVE_ACCESSOR(Static);

#define DEFINE_PRELUDE_ACCESSOR(name) \
    ObjectPtr prelude_##name() { \
        static ObjectPtr cached; \
        if (!cached) \
            cached = safeLookupPublic(preludeModule(), fnameToIdent(#name)); \
        return cached; \
    } \
    \
    ExprPtr prelude_expr_##name() { \
        static ExprPtr cached; \
        if (!cached) \
            cached = new ObjectExpr(prelude_##name()); \
        return cached; \
    }

DEFINE_PRELUDE_ACCESSOR(dereference);
DEFINE_PRELUDE_ACCESSOR(plus);
DEFINE_PRELUDE_ACCESSOR(minus);
DEFINE_PRELUDE_ACCESSOR(add);
DEFINE_PRELUDE_ACCESSOR(subtract);
DEFINE_PRELUDE_ACCESSOR(multiply);
DEFINE_PRELUDE_ACCESSOR(divide);
DEFINE_PRELUDE_ACCESSOR(remainder);
DEFINE_PRELUDE_ACCESSOR(equalsP);
DEFINE_PRELUDE_ACCESSOR(notEqualsP);
DEFINE_PRELUDE_ACCESSOR(lesserP);
DEFINE_PRELUDE_ACCESSOR(lesserEqualsP);
DEFINE_PRELUDE_ACCESSOR(greaterP);
DEFINE_PRELUDE_ACCESSOR(greaterEqualsP);
DEFINE_PRELUDE_ACCESSOR(tupleLiteral);
DEFINE_PRELUDE_ACCESSOR(Array);
DEFINE_PRELUDE_ACCESSOR(staticIndex);
DEFINE_PRELUDE_ACCESSOR(index);
DEFINE_PRELUDE_ACCESSOR(fieldRef);
DEFINE_PRELUDE_ACCESSOR(call);
DEFINE_PRELUDE_ACCESSOR(destroy);
DEFINE_PRELUDE_ACCESSOR(move);
DEFINE_PRELUDE_ACCESSOR(assign);
DEFINE_PRELUDE_ACCESSOR(addAssign);
DEFINE_PRELUDE_ACCESSOR(subtractAssign);
DEFINE_PRELUDE_ACCESSOR(multiplyAssign);
DEFINE_PRELUDE_ACCESSOR(divideAssign);
DEFINE_PRELUDE_ACCESSOR(remainderAssign);
DEFINE_PRELUDE_ACCESSOR(ByRef);
DEFINE_PRELUDE_ACCESSOR(setArgcArgv);
DEFINE_PRELUDE_ACCESSOR(callMain);
DEFINE_PRELUDE_ACCESSOR(Char);
DEFINE_PRELUDE_ACCESSOR(allocateShared);
DEFINE_PRELUDE_ACCESSOR(wrapStatic);
DEFINE_PRELUDE_ACCESSOR(iterator);
DEFINE_PRELUDE_ACCESSOR(hasNextP);
DEFINE_PRELUDE_ACCESSOR(next);
DEFINE_PRELUDE_ACCESSOR(throwValue);
DEFINE_PRELUDE_ACCESSOR(exceptionIsP);
DEFINE_PRELUDE_ACCESSOR(exceptionAs);
DEFINE_PRELUDE_ACCESSOR(exceptionAsAny);
DEFINE_PRELUDE_ACCESSOR(continueException);
DEFINE_PRELUDE_ACCESSOR(unhandledExceptionInExternal);
DEFINE_PRELUDE_ACCESSOR(packMultiValuedFreeVarByRef);
DEFINE_PRELUDE_ACCESSOR(packMultiValuedFreeVar);
DEFINE_PRELUDE_ACCESSOR(unpackMultiValuedFreeVarAndDereference);
DEFINE_PRELUDE_ACCESSOR(unpackMultiValuedFreeVar);
DEFINE_PRELUDE_ACCESSOR(VariantRepr);
DEFINE_PRELUDE_ACCESSOR(variantTag);
DEFINE_PRELUDE_ACCESSOR(unsafeVariantIndex);
DEFINE_PRELUDE_ACCESSOR(invalidVariant);
DEFINE_PRELUDE_ACCESSOR(StringConstant);
