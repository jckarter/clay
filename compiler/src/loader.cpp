#include "clay.hpp"

namespace clay {

using namespace std;

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif


static vector<string> searchPath;
static vector<string> moduleSuffixes;

map<string, ModulePtr> globalModules;
map<string, string> globalFlags;
ModulePtr globalMainModule;


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

static std::string getPtrSize(const llvm::DataLayout *dataLayout) {
    switch (dataLayout->getPointerSizeInBits()) {
    case 32 : return "32";
    case 64 : return "64";
    default : assert(false); return "";
    }
}

static void initModuleSuffixes() {
    llvm::Triple triple(llvmModule->getTargetTriple());

    string os = getOS(triple);
    string osgroup = getOSGroup(triple);
    string cpu = getCPU(triple);
    string bits = getPtrSize(llvmDataLayout);
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
// toKey
//

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
// addSearchPath, locateFile, toRelativePath1, toRelativePath2,
// locateModule
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

static string toRelativePath1(DottedNamePtr name) {
    string relativePath;
    for (unsigned i = 0; i < name->parts.size(); ++i) {
        relativePath.append(name->parts[i]->str);
        relativePath.push_back(PATH_SEPARATOR);
    }
    relativePath.append(name->parts.back()->str);
    // relative path has no suffix
    return relativePath;
}

static string toRelativePath2(DottedNamePtr name) {
    string relativePath;
    for (unsigned i = 0; i+1 < name->parts.size(); ++i) {
        relativePath.append(name->parts[i]->str);
        relativePath.push_back(PATH_SEPARATOR);
    }
    relativePath.append(name->parts.back()->str);
    // relative path has no suffix
    return relativePath;
}

static string locateModule(DottedNamePtr name) {
    string path, relativePath;

    relativePath = toRelativePath1(name);
    if (locateFile(relativePath, path))
        return path;

    relativePath = toRelativePath2(name);
    if (locateFile(relativePath, path))
        return path;

    error(name, "module not found: " + toKey(name));
    return "";
}



//
// loadFile
//

static SourcePtr loadFile(const string &fileName, vector<string> *sourceFiles) {
    if (sourceFiles != NULL)
        sourceFiles->push_back(fileName);
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

    SourcePtr src = new Source(fileName, buf, size);
    if (llvmDIBuilder != NULL) {
        llvm::SmallString<260> absFileName(fileName);
        llvm::sys::fs::make_absolute(absFileName);
        src->debugInfo = (llvm::MDNode*)llvmDIBuilder->createFile(
            llvm::sys::path::filename(absFileName),
            llvm::sys::path::parent_path(absFileName));
    }
    return src;
}



//
// loadModuleByName, loadDependents, loadProgram
//

static void loadDependents(ModulePtr m, vector<string> *sourceFiles);
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
        case ENUMERATION : {
            Enumeration *enumer = (Enumeration *)x;
            TypePtr t = enumType(enumer);
            addGlobal(m, enumer->name, enumer->visibility, t.ptr());
            for (unsigned i = 0 ; i < enumer->members.size(); ++i) {
                EnumMember *member = enumer->members[i].ptr();
                member->index = (int)i;
                member->type = t;
                addGlobal(m, member->name, enumer->visibility, member);
            }
            break;
        }
        case PROCEDURE : {
            Procedure *proc = (Procedure *)x;
            if (proc->interface != NULL)
                proc->interface->env = m->env;
        }
        default :
            if (x->name.ptr())
                addGlobal(m, x->name, x->visibility, x);
            break;
        }
    }
}

static ModulePtr loadModuleByName(DottedNamePtr name, vector<string> *sourceFiles) {
    string key = toKey(name);

    map<string, ModulePtr>::iterator i = globalModules.find(key);
    if (i != globalModules.end())
        return i->second;

    ModulePtr module;

    if (key == "__primitives__") {
        module = makePrimitivesModule();
    }
    else {
        string path = locateModule(name);
        module = parse(key, loadFile(path, sourceFiles));
    }

    globalModules[key] = module;
    loadDependents(module, sourceFiles);
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

static void loadDependents(ModulePtr m, vector<string> *sourceFiles) {
    m->rootHolder = new ModuleHolder();
    m->publicRootHolder = new ModuleHolder();
    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii) {
        ImportPtr x = *ii;
        x->module = loadModuleByName(x->dottedName, sourceFiles);
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

static ModulePtr loadPrelude(vector<string> *sourceFiles) {
    DottedNamePtr dottedName = new DottedName();
    dottedName->parts.push_back(new Identifier("prelude"));
    return loadModuleByName(dottedName, sourceFiles);
}

ModulePtr loadProgram(const string &fileName, vector<string> *sourceFiles) {
    globalMainModule = parse("", loadFile(fileName, sourceFiles));
    ModulePtr prelude = loadPrelude(sourceFiles);
    loadDependents(globalMainModule, sourceFiles);
    installGlobals(globalMainModule);
    initModule(prelude);
    initModule(globalMainModule);
    return globalMainModule;
}

ModulePtr loadProgramSource(const string &name, const string &source) {
    globalMainModule = parse("", new Source(name,
        const_cast<char*>(source.c_str()),
        source.size())
    );
    // Don't keep track of source files for -e script
    ModulePtr prelude = loadPrelude(NULL);
    loadDependents(globalMainModule, NULL);
    installGlobals(globalMainModule);
    initModule(prelude);
    initModule(globalMainModule);
    return globalMainModule;
}

ModulePtr loadedModule(const string &module) {
    if (!globalModules.count(module))
        error("module not loaded: " + module);
    return globalModules[module];
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
        x->nameIsPattern = true;
        addPatternOverload(x);
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
            Type *z = (Type *)y.ptr();
            z->overloads.insert(z->overloads.begin(), x);
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

    if (m->declaration != NULL) {
        if (m->moduleName == "")
            m->moduleName = toKey(m->declaration->name);
        else if (m->moduleName != toKey(m->declaration->name))
            error(m->declaration,
                "module imported by name " + m->moduleName
                + " but declared with name " + toKey(m->declaration->name)
            );
    } else if (m->moduleName == "")
        m->moduleName = "__main__";

    verifyAttributes(m);

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
        }
    }

    if (llvmDIBuilder != NULL) {
        llvm::DIFile file = m->location == NULL
            ? llvm::DIFile(NULL)
            : m->location->source->getDebugInfo();
        m->debugInfo = (llvm::MDNode*)llvmDIBuilder->createNameSpace(
            llvm::DICompileUnit(llvmDIBuilder->getCU()), // scope
            m->moduleName, // name
            file, // file
            1 // line
            );
    }
}



//
// staticModule
//

static ModulePtr envModule(EnvPtr env) {
    if (env == NULL || env->parent == NULL)
        return NULL;
    switch (env->parent->objKind) {
    case ENV :
        return envModule((Env *)env->parent.ptr());
    case MODULE :
        return (Module *)env->parent.ptr();
    default :
        assert(false);
        return NULL;
    }
}

static ModulePtr typeModule(TypePtr t) {
    switch (t->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case COMPLEX_TYPE :
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
    case ARRAY_TYPE :
    case VEC_TYPE :
    case TUPLE_TYPE :
    case UNION_TYPE :
    case STATIC_TYPE :
        return primitivesModule();
    case RECORD_TYPE : {
        RecordType *rt = (RecordType *)t.ptr();
        return envModule(rt->record->env);
    }
    case VARIANT_TYPE : {
        VariantType *vt = (VariantType *)t.ptr();
        return envModule(vt->variant->env);
    }
    case ENUM_TYPE : {
        EnumType *et = (EnumType *)t.ptr();
        return envModule(et->enumeration->env);
    }
    default :
        return NULL;
    }
}

ModulePtr staticModule(ObjectPtr x) {
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        return typeModule(y);
    }
    case PRIM_OP : {
        return primitivesModule();
    }
    case MODULE_HOLDER : {
        ModuleHolder *mh = (ModuleHolder *)x.ptr();
        if (mh->import.ptr())
            return mh->import->module;
        return NULL;
    }
    case GLOBAL_VARIABLE :
    case GLOBAL_ALIAS :
    case EXTERNAL_PROCEDURE :
    case EXTERNAL_VARIABLE :
    case PROCEDURE :
    case RECORD :
    case VARIANT : {
        TopLevelItem *t = (TopLevelItem *)x.ptr();
        return envModule(t->env);
    }
    default :
        return NULL;
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
    addPrim(prims, "Int128", int128Type.ptr());
    addPrim(prims, "UInt8", uint8Type.ptr());
    addPrim(prims, "UInt16", uint16Type.ptr());
    addPrim(prims, "UInt32", uint32Type.ptr());
    addPrim(prims, "UInt64", uint64Type.ptr());
    addPrim(prims, "UInt128", uint128Type.ptr());
    addPrim(prims, "Float32", float32Type.ptr());
    addPrim(prims, "Float64", float64Type.ptr());
    addPrim(prims, "Float80", float80Type.ptr());
    addPrim(prims, "Imag32", imag32Type.ptr());
    addPrim(prims, "Imag64", imag64Type.ptr());
    addPrim(prims, "Imag80", imag80Type.ptr());
    addPrim(prims, "Complex32", complex32Type.ptr());
    addPrim(prims, "Complex64", complex64Type.ptr());
    addPrim(prims, "Complex80", complex80Type.ptr());

    GlobalAliasPtr v =
        new GlobalAlias(new Identifier("ExceptionsEnabled?"),
                        PUBLIC,
                        vector<PatternVar>(),
                        NULL,
                        vector<IdentifierPtr>(),
                        NULL,
                        new BoolLiteral(exceptionsEnabled()));
    addPrim(prims, "ExceptionsEnabled?", v.ptr());

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

    PRIMITIVE(integerAddChecked);
    PRIMITIVE(integerSubtractChecked);
    PRIMITIVE(integerMultiplyChecked);
    PRIMITIVE(integerDivideChecked);
    PRIMITIVE(integerRemainderChecked);
    PRIMITIVE(integerShiftLeftChecked);
    PRIMITIVE(integerNegateChecked);
    PRIMITIVE(integerConvertChecked);

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
    PRIMITIVE(AttributeThisCall);
    PRIMITIVE(AttributeLLVMCall);
    PRIMITIVE(AttributeDLLImport);
    PRIMITIVE(AttributeDLLExport);

    PRIMITIVE(CCodePointerP);
    PRIMITIVE(CCodePointer);
    PRIMITIVE(VarArgsCCodePointer);
    PRIMITIVE(StdCallCodePointer);
    PRIMITIVE(FastCallCodePointer);
    PRIMITIVE(ThisCallCodePointer);
    PRIMITIVE(LLVMCodePointer);
    PRIMITIVE(makeCCodePointer);
    PRIMITIVE(callCCodePointer);

    PRIMITIVE(pointerCast);

    PRIMITIVE(Array);
    PRIMITIVE(arrayRef);
    PRIMITIVE(arrayElements);

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
    PRIMITIVE(RecordWithFieldP);
    PRIMITIVE(recordFieldRef);
    PRIMITIVE(recordFieldRefByName);
    PRIMITIVE(recordFields);

    PRIMITIVE(VariantP);
    PRIMITIVE(VariantMemberIndex);
    PRIMITIVE(VariantMemberCount);
    PRIMITIVE(variantRepr);

    PRIMITIVE(Static);
    PRIMITIVE(ModuleName);
    PRIMITIVE(StaticName);
    PRIMITIVE(staticIntegers);
    PRIMITIVE(staticFieldRef);

    PRIMITIVE(EnumP);
    PRIMITIVE(EnumMemberCount);
    PRIMITIVE(EnumMemberName);
    PRIMITIVE(enumToInt);
    PRIMITIVE(intToEnum);

    PRIMITIVE(IdentifierP);
    PRIMITIVE(IdentifierSize);
    PRIMITIVE(IdentifierConcat);
    PRIMITIVE(IdentifierSlice);
    PRIMITIVE(IdentifierModuleName);
    PRIMITIVE(IdentifierStaticName);

    PRIMITIVE(OrderUnordered);
    PRIMITIVE(OrderMonotonic);
    PRIMITIVE(OrderAcquire);
    PRIMITIVE(OrderRelease);
    PRIMITIVE(OrderAcqRel);
    PRIMITIVE(OrderSeqCst);

    PRIMITIVE(FlagP);
    PRIMITIVE(Flag);

    PRIMITIVE(atomicFence);
    PRIMITIVE(atomicRMW);
    PRIMITIVE(atomicLoad);
    PRIMITIVE(atomicStore);
    PRIMITIVE(atomicCompareExchange);

    PRIMITIVE(RMWXchg);
    PRIMITIVE(RMWAdd);
    PRIMITIVE(RMWSubtract);
    PRIMITIVE(RMWAnd);
    PRIMITIVE(RMWNAnd);
    PRIMITIVE(RMWOr);
    PRIMITIVE(RMWXor);
    PRIMITIVE(RMWMin);
    PRIMITIVE(RMWMax);
    PRIMITIVE(RMWUMin);
    PRIMITIVE(RMWUMax);

    PRIMITIVE(activeException);

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

static ObjectPtr convertObject(ObjectPtr x) {
    switch (x->objKind) {
    case RECORD : {
        Record *y = (Record *)x.ptr();
        if (y->params.empty() && !y->varParam.ptr())
            return recordType(y, vector<ObjectPtr>()).ptr();
        return x;
    }
    case VARIANT : {
        Variant *y = (Variant *)x.ptr();
        if (y->params.empty() && !y->varParam.ptr())
            return variantType(y, vector<ObjectPtr>()).ptr();
        return x;
    }
    default :
        return x;
    }
}

#define DEFINE_PRIMITIVE_ACCESSOR(name) \
    ObjectPtr primitive_##name() { \
        static ObjectPtr cached; \
        if (!cached) { \
            cached = safeLookupPublic(primitivesModule(), fnameToIdent(#name)); \
            cached = convertObject(cached); \
        } \
        return cached; \
    } \
    \
    ExprPtr primitive_expr_##name() { \
        static ExprPtr cached; \
        if (!cached) \
            cached = new ObjectExpr(primitive_##name()); \
        return cached; \
    }

DEFINE_PRIMITIVE_ACCESSOR(addressOf)
DEFINE_PRIMITIVE_ACCESSOR(boolNot)
DEFINE_PRIMITIVE_ACCESSOR(Pointer)
DEFINE_PRIMITIVE_ACCESSOR(CodePointer)
DEFINE_PRIMITIVE_ACCESSOR(CCodePointer)
DEFINE_PRIMITIVE_ACCESSOR(VarArgsCCodePointer)
DEFINE_PRIMITIVE_ACCESSOR(StdCallCodePointer)
DEFINE_PRIMITIVE_ACCESSOR(FastCallCodePointer)
DEFINE_PRIMITIVE_ACCESSOR(ThisCallCodePointer)
DEFINE_PRIMITIVE_ACCESSOR(LLVMCodePointer)
DEFINE_PRIMITIVE_ACCESSOR(Array)
DEFINE_PRIMITIVE_ACCESSOR(Vec)
DEFINE_PRIMITIVE_ACCESSOR(Tuple)
DEFINE_PRIMITIVE_ACCESSOR(Union)
DEFINE_PRIMITIVE_ACCESSOR(Static)
DEFINE_PRIMITIVE_ACCESSOR(activeException)

#define DEFINE_PRELUDE_ACCESSOR(name) \
    ObjectPtr prelude_##name() { \
        static ObjectPtr cached; \
        if (!cached) { \
            cached = safeLookupPublic(preludeModule(), fnameToIdent(#name)); \
            cached = convertObject(cached); \
        } \
        return cached; \
    } \
    \
    ExprPtr prelude_expr_##name() { \
        static ExprPtr cached; \
        if (!cached) \
            cached = new ObjectExpr(prelude_##name()); \
        return cached; \
    }

DEFINE_PRELUDE_ACCESSOR(dereference)
DEFINE_PRELUDE_ACCESSOR(plus)
DEFINE_PRELUDE_ACCESSOR(minus)
DEFINE_PRELUDE_ACCESSOR(add)
DEFINE_PRELUDE_ACCESSOR(subtract)
DEFINE_PRELUDE_ACCESSOR(multiply)
DEFINE_PRELUDE_ACCESSOR(divide)
DEFINE_PRELUDE_ACCESSOR(remainder)
DEFINE_PRELUDE_ACCESSOR(caseP)
DEFINE_PRELUDE_ACCESSOR(equalsP)
DEFINE_PRELUDE_ACCESSOR(notEqualsP)
DEFINE_PRELUDE_ACCESSOR(lesserP)
DEFINE_PRELUDE_ACCESSOR(lesserEqualsP)
DEFINE_PRELUDE_ACCESSOR(greaterP)
DEFINE_PRELUDE_ACCESSOR(greaterEqualsP)
DEFINE_PRELUDE_ACCESSOR(tupleLiteral)
DEFINE_PRELUDE_ACCESSOR(staticIndex)
DEFINE_PRELUDE_ACCESSOR(index)
DEFINE_PRELUDE_ACCESSOR(fieldRef)
DEFINE_PRELUDE_ACCESSOR(call)
DEFINE_PRELUDE_ACCESSOR(destroy)
DEFINE_PRELUDE_ACCESSOR(move)
DEFINE_PRELUDE_ACCESSOR(assign)
DEFINE_PRELUDE_ACCESSOR(updateAssign)
DEFINE_PRELUDE_ACCESSOR(indexAssign)
DEFINE_PRELUDE_ACCESSOR(indexUpdateAssign)
DEFINE_PRELUDE_ACCESSOR(fieldRefAssign)
DEFINE_PRELUDE_ACCESSOR(fieldRefUpdateAssign)
DEFINE_PRELUDE_ACCESSOR(staticIndexAssign)
DEFINE_PRELUDE_ACCESSOR(staticIndexUpdateAssign)
DEFINE_PRELUDE_ACCESSOR(ByRef)
DEFINE_PRELUDE_ACCESSOR(setArgcArgv)
DEFINE_PRELUDE_ACCESSOR(callMain)
DEFINE_PRELUDE_ACCESSOR(Char)
DEFINE_PRELUDE_ACCESSOR(wrapStatic)
DEFINE_PRELUDE_ACCESSOR(iterator)
DEFINE_PRELUDE_ACCESSOR(hasNextP)
DEFINE_PRELUDE_ACCESSOR(next)
DEFINE_PRELUDE_ACCESSOR(throwValue)
DEFINE_PRELUDE_ACCESSOR(exceptionIsP)
DEFINE_PRELUDE_ACCESSOR(exceptionAs)
DEFINE_PRELUDE_ACCESSOR(exceptionAsAny)
DEFINE_PRELUDE_ACCESSOR(continueException)
DEFINE_PRELUDE_ACCESSOR(unhandledExceptionInExternal)
DEFINE_PRELUDE_ACCESSOR(exceptionInInitializer)
DEFINE_PRELUDE_ACCESSOR(exceptionInFinalizer)
DEFINE_PRELUDE_ACCESSOR(packMultiValuedFreeVarByRef)
DEFINE_PRELUDE_ACCESSOR(packMultiValuedFreeVar)
DEFINE_PRELUDE_ACCESSOR(unpackMultiValuedFreeVarAndDereference)
DEFINE_PRELUDE_ACCESSOR(unpackMultiValuedFreeVar)
DEFINE_PRELUDE_ACCESSOR(VariantRepr)
DEFINE_PRELUDE_ACCESSOR(variantTag)
DEFINE_PRELUDE_ACCESSOR(unsafeVariantIndex)
DEFINE_PRELUDE_ACCESSOR(invalidVariant)
DEFINE_PRELUDE_ACCESSOR(StringConstant)
DEFINE_PRELUDE_ACCESSOR(ifExpression)
DEFINE_PRELUDE_ACCESSOR(RecordWithProperties)
DEFINE_PRELUDE_ACCESSOR(typeToRValue)
DEFINE_PRELUDE_ACCESSOR(typesToRValues)
DEFINE_PRELUDE_ACCESSOR(doIntegerAddChecked);
DEFINE_PRELUDE_ACCESSOR(doIntegerSubtractChecked);
DEFINE_PRELUDE_ACCESSOR(doIntegerMultiplyChecked);
DEFINE_PRELUDE_ACCESSOR(doIntegerDivideChecked);
DEFINE_PRELUDE_ACCESSOR(doIntegerRemainderChecked);
DEFINE_PRELUDE_ACCESSOR(doIntegerShiftLeftChecked);
DEFINE_PRELUDE_ACCESSOR(doIntegerNegateChecked);
DEFINE_PRELUDE_ACCESSOR(doIntegerConvertChecked);

}
