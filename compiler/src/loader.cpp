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

static std::string getPtrSize(const llvm::TargetData *targetData) {
    switch (targetData->getPointerSizeInBits()) {
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
static ModulePtr makeOperatorsModule();

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
            // fallthrough
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
    } else if (key == "__operators__") {
        module = makeOperatorsModule();
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

static EnvPtr overloadPatternEnv(OverloadPtr x) {
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
    return env;
}

void getProcedureMonoTypes(ProcedureMono &mono, EnvPtr env,
    vector<FormalArgPtr> const &formalArgs, FormalArgPtr formalVarArg)
{
    if (mono.monoState == Procedure_NoOverloads && formalVarArg == NULL)
    {
        assert(mono.monoTypes.empty());
        mono.monoState = Procedure_MonoOverload;
        for (size_t i = 0; i < formalArgs.size(); ++i) {
            if (formalArgs[i]->type == NULL)
                goto poly;
            PatternPtr argPattern =
                evaluateOnePattern(formalArgs[i]->type, env);
            ObjectPtr argType = derefDeep(argPattern);
            if (argType == NULL)
                goto poly;
            if (argType->objKind != TYPE)
                error(formalArgs[i], "expecting a type");

            mono.monoTypes.push_back((Type*)argType.ptr());
        }
    } else
        goto poly;
    return;

poly:
    mono.monoTypes.clear();
    mono.monoState = Procedure_PolyOverload;
    return;
}

void addProcedureOverload(ProcedurePtr proc, OverloadPtr x) {
    proc->overloads.insert(proc->overloads.begin(), x);
    getProcedureMonoTypes(proc->mono, overloadPatternEnv(x),
        x->code->formalArgs,
        x->code->formalVarArg);
}

static void initOverload(OverloadPtr x) {
    EnvPtr env = overloadPatternEnv(x);
    PatternPtr pattern = evaluateOnePattern(x->target, env);
    ObjectPtr obj = derefDeep(pattern);
    if (obj == NULL) {
        x->nameIsPattern = true;
        addPatternOverload(x);
    }
    else {
        switch (obj->objKind) {
        case PROCEDURE : {
            Procedure *proc = (Procedure *)obj.ptr();
            addProcedureOverload(proc, x);
            break;
        }
        case RECORD : {
            Record *r = (Record *)obj.ptr();
            r->overloads.insert(r->overloads.begin(), x);
            break;
        }
        case VARIANT : {
            Variant *v = (Variant *)obj.ptr();
            v->overloads.insert(v->overloads.begin(), x);
            break;
        }
        case TYPE : {
            Type *t = (Type *)obj.ptr();
            t->overloads.insert(t->overloads.begin(), x);
            break;
        }
        case PRIM_OP : {
            if (isOverloadablePrimOp(obj))
                addPrimOpOverload((PrimOp *)obj.ptr(), x);
            else
                error(x->target, "invalid overload target");
            break;
        }
        case GLOBAL_ALIAS : {
            GlobalAlias *a = (GlobalAlias*)obj.ptr();
            if (!a->hasParams())
                error(x->target, "invalid overload target");
            a->overloads.insert(a->overloads.begin(), x);
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

    vector<IdentifierPtr> recordParams;
    RecordBodyPtr recordBody = new RecordBody(vector<RecordFieldPtr>());
    recordParams.push_back(new Identifier("T"));
    RecordPtr byRefRecord = new Record(new Identifier("ByRef"),
        PUBLIC,
        vector<PatternVar>(),
        NULL,
        recordParams,
        NULL,
        recordBody);
    byRefRecord->env = new Env(prims);
    addPrim(prims, "ByRef", byRefRecord.ptr());

    recordParams.clear();
    recordParams.push_back(new Identifier("Properties"));
    recordParams.push_back(new Identifier("Fields"));
    RecordPtr rwpRecord = new Record(new Identifier("RecordWithProperties"),
        PUBLIC,
        vector<PatternVar>(),
        NULL,
        recordParams,
        NULL,
        recordBody);
    rwpRecord->env = new Env(prims);
    addPrim(prims, "RecordWithProperties", rwpRecord.ptr());

#define PRIMITIVE(x) addPrimOp(prims, toPrimStr(#x), new PrimOp(PRIM_##x))

    PRIMITIVE(TypeP);
    PRIMITIVE(TypeSize);
    PRIMITIVE(TypeAlignment);

    PRIMITIVE(StaticCallDefinedP);
    PRIMITIVE(StaticCallOutputTypes);

    PRIMITIVE(StaticMonoP);
    PRIMITIVE(StaticMonoInputTypes);

    PRIMITIVE(bitcopy);
    PRIMITIVE(bitcast);

    PRIMITIVE(boolNot);

    PRIMITIVE(integerEqualsP);
    PRIMITIVE(integerLesserP);

    PRIMITIVE(numericAdd);
    PRIMITIVE(numericSubtract);
    PRIMITIVE(numericMultiply);
    PRIMITIVE(floatDivide);
    PRIMITIVE(numericNegate);

    PRIMITIVE(integerQuotient);
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
    PRIMITIVE(integerQuotientChecked);
    PRIMITIVE(integerRemainderChecked);
    PRIMITIVE(integerShiftLeftChecked);
    PRIMITIVE(integerNegateChecked);
    PRIMITIVE(integerConvertChecked);

    PRIMITIVE(floatOrderedEqualsP);
    PRIMITIVE(floatOrderedLesserP);
    PRIMITIVE(floatOrderedLesserEqualsP);
    PRIMITIVE(floatOrderedGreaterP);
    PRIMITIVE(floatOrderedGreaterEqualsP);
    PRIMITIVE(floatOrderedNotEqualsP);
    PRIMITIVE(floatOrderedP);
    PRIMITIVE(floatUnorderedEqualsP);
    PRIMITIVE(floatUnorderedLesserP);
    PRIMITIVE(floatUnorderedLesserEqualsP);
    PRIMITIVE(floatUnorderedGreaterP);
    PRIMITIVE(floatUnorderedGreaterEqualsP);
    PRIMITIVE(floatUnorderedNotEqualsP);
    PRIMITIVE(floatUnorderedP);

    PRIMITIVE(Pointer);

    PRIMITIVE(addressOf);
    PRIMITIVE(pointerDereference);
    PRIMITIVE(pointerOffset);
    PRIMITIVE(pointerToInt);
    PRIMITIVE(intToPointer);
    PRIMITIVE(nullPointer);

    PRIMITIVE(CodePointer);
    PRIMITIVE(makeCodePointer);

    PRIMITIVE(AttributeStdCall);
    PRIMITIVE(AttributeFastCall);
    PRIMITIVE(AttributeCCall);
    PRIMITIVE(AttributeThisCall);
    PRIMITIVE(AttributeLLVMCall);
    PRIMITIVE(AttributeDLLImport);
    PRIMITIVE(AttributeDLLExport);

    PRIMITIVE(ExternalCodePointer);
    PRIMITIVE(makeExternalCodePointer);
    PRIMITIVE(callExternalCodePointer);

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
    PRIMITIVE(VariantMembers);
    PRIMITIVE(variantRepr);

    PRIMITIVE(Static);
    PRIMITIVE(ModuleName);
    PRIMITIVE(StaticName);
    PRIMITIVE(staticIntegers);
    PRIMITIVE(integers);
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

    PRIMITIVE(memcpy);
    PRIMITIVE(memmove);

    PRIMITIVE(countValues);
    PRIMITIVE(nthValue);
    PRIMITIVE(withoutNthValue);
    PRIMITIVE(takeValues);
    PRIMITIVE(dropValues);

    PRIMITIVE(LambdaRecordP);
    PRIMITIVE(LambdaSymbolP);
    PRIMITIVE(LambdaMonoP);
    PRIMITIVE(LambdaMonoInputTypes);

    PRIMITIVE(GetOverload);

#undef PRIMITIVE

    return prims;
}

static void addOperator(ModulePtr module, const string &name) {
    IdentifierPtr ident = new Identifier(name);
    ProcedurePtr opProc = new Procedure(ident, PUBLIC);
    opProc->env = new Env(module);
    addGlobal(module, ident, PUBLIC, opProc.ptr());
}

static ModulePtr makeOperatorsModule() {
    ModulePtr operators = new Module("__operators__");

#define OPERATOR(x) addOperator(operators, toPrimStr(#x))

    OPERATOR(dereference);
    OPERATOR(plus);
    OPERATOR(minus);
    OPERATOR(add);
    OPERATOR(subtract);
    OPERATOR(multiply);
    OPERATOR(divide);
    OPERATOR(quotient);
    OPERATOR(remainder);
    OPERATOR(caseP);
    OPERATOR(equalsP);
    OPERATOR(notEqualsP);
    OPERATOR(lesserP);
    OPERATOR(lesserEqualsP);
    OPERATOR(greaterP);
    OPERATOR(greaterEqualsP);
    OPERATOR(cat);
    OPERATOR(tupleLiteral);
    OPERATOR(staticIndex);
    OPERATOR(index);
    OPERATOR(fieldRef);
    OPERATOR(call);
    OPERATOR(destroy);
    OPERATOR(copy);
    OPERATOR(move);
    OPERATOR(assign);
    OPERATOR(updateAssign);
    OPERATOR(indexAssign);
    OPERATOR(indexUpdateAssign);
    OPERATOR(fieldRefAssign);
    OPERATOR(fieldRefUpdateAssign);
    OPERATOR(staticIndexAssign);
    OPERATOR(staticIndexUpdateAssign);
    OPERATOR(callMain);
    OPERATOR(charLiteral);
    OPERATOR(wrapStatic);
    OPERATOR(iterator);
    OPERATOR(hasNextP);
    OPERATOR(next);
    OPERATOR(throwValue);
    OPERATOR(exceptionIsP);
    OPERATOR(exceptionAs);
    OPERATOR(exceptionAsAny);
    OPERATOR(continueException);
    OPERATOR(unhandledExceptionInExternal);
    OPERATOR(exceptionInInitializer);
    OPERATOR(exceptionInFinalizer);
    OPERATOR(packMultiValuedFreeVarByRef);
    OPERATOR(packMultiValuedFreeVar);
    OPERATOR(unpackMultiValuedFreeVarAndDereference);
    OPERATOR(unpackMultiValuedFreeVar);
    OPERATOR(variantReprType);
    OPERATOR(DispatchTagCount);
    OPERATOR(dispatchTag);
    OPERATOR(dispatchIndex);
    OPERATOR(invalidDispatch);
    OPERATOR(stringLiteral);
    OPERATOR(ifExpression);
    OPERATOR(typeToRValue);
    OPERATOR(typesToRValues);
    OPERATOR(doIntegerAddChecked);
    OPERATOR(doIntegerSubtractChecked);
    OPERATOR(doIntegerMultiplyChecked);
    OPERATOR(dointegerQuotientChecked);
    OPERATOR(doIntegerRemainderChecked);
    OPERATOR(doIntegerShiftLeftChecked);
    OPERATOR(doIntegerNegateChecked);
    OPERATOR(doIntegerConvertChecked);

#undef OPERATOR

    return operators;
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

ModulePtr operatorsModule() {
    static ModulePtr cached;
    if (!cached)
        cached = loadedModule("__operators__");
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
DEFINE_PRIMITIVE_ACCESSOR(ExternalCodePointer)
DEFINE_PRIMITIVE_ACCESSOR(AttributeCCall);
DEFINE_PRIMITIVE_ACCESSOR(AttributeStdCall);
DEFINE_PRIMITIVE_ACCESSOR(AttributeFastCall);
DEFINE_PRIMITIVE_ACCESSOR(AttributeThisCall);
DEFINE_PRIMITIVE_ACCESSOR(AttributeLLVMCall);
DEFINE_PRIMITIVE_ACCESSOR(Array)
DEFINE_PRIMITIVE_ACCESSOR(Vec)
DEFINE_PRIMITIVE_ACCESSOR(Tuple)
DEFINE_PRIMITIVE_ACCESSOR(Union)
DEFINE_PRIMITIVE_ACCESSOR(Static)
DEFINE_PRIMITIVE_ACCESSOR(activeException)
DEFINE_PRIMITIVE_ACCESSOR(ByRef)
DEFINE_PRIMITIVE_ACCESSOR(RecordWithProperties)

#define DEFINE_OPERATOR_ACCESSOR(name) \
    ObjectPtr operator_##name() { \
        static ObjectPtr cached; \
        if (!cached) { \
            cached = safeLookupPublic(operatorsModule(), fnameToIdent(#name)); \
            cached = convertObject(cached); \
        } \
        return cached; \
    } \
    \
    ExprPtr operator_expr_##name() { \
        static ExprPtr cached; \
        if (!cached) \
            cached = new ObjectExpr(operator_##name()); \
        return cached; \
    }

DEFINE_OPERATOR_ACCESSOR(dereference)
DEFINE_OPERATOR_ACCESSOR(plus)
DEFINE_OPERATOR_ACCESSOR(minus)
DEFINE_OPERATOR_ACCESSOR(add)
DEFINE_OPERATOR_ACCESSOR(subtract)
DEFINE_OPERATOR_ACCESSOR(multiply)
DEFINE_OPERATOR_ACCESSOR(divide)
DEFINE_OPERATOR_ACCESSOR(quotient)
DEFINE_OPERATOR_ACCESSOR(remainder)
DEFINE_OPERATOR_ACCESSOR(caseP)
DEFINE_OPERATOR_ACCESSOR(equalsP)
DEFINE_OPERATOR_ACCESSOR(notEqualsP)
DEFINE_OPERATOR_ACCESSOR(lesserP)
DEFINE_OPERATOR_ACCESSOR(lesserEqualsP)
DEFINE_OPERATOR_ACCESSOR(greaterP)
DEFINE_OPERATOR_ACCESSOR(greaterEqualsP)
DEFINE_OPERATOR_ACCESSOR(cat)
DEFINE_OPERATOR_ACCESSOR(tupleLiteral)
DEFINE_OPERATOR_ACCESSOR(staticIndex)
DEFINE_OPERATOR_ACCESSOR(index)
DEFINE_OPERATOR_ACCESSOR(fieldRef)
DEFINE_OPERATOR_ACCESSOR(call)
DEFINE_OPERATOR_ACCESSOR(destroy)
DEFINE_OPERATOR_ACCESSOR(copy)
DEFINE_OPERATOR_ACCESSOR(move)
DEFINE_OPERATOR_ACCESSOR(assign)
DEFINE_OPERATOR_ACCESSOR(updateAssign)
DEFINE_OPERATOR_ACCESSOR(indexAssign)
DEFINE_OPERATOR_ACCESSOR(indexUpdateAssign)
DEFINE_OPERATOR_ACCESSOR(fieldRefAssign)
DEFINE_OPERATOR_ACCESSOR(fieldRefUpdateAssign)
DEFINE_OPERATOR_ACCESSOR(staticIndexAssign)
DEFINE_OPERATOR_ACCESSOR(staticIndexUpdateAssign)
DEFINE_OPERATOR_ACCESSOR(callMain)
DEFINE_OPERATOR_ACCESSOR(charLiteral)
DEFINE_OPERATOR_ACCESSOR(iterator)
DEFINE_OPERATOR_ACCESSOR(hasNextP)
DEFINE_OPERATOR_ACCESSOR(next)
DEFINE_OPERATOR_ACCESSOR(throwValue)
DEFINE_OPERATOR_ACCESSOR(exceptionIsP)
DEFINE_OPERATOR_ACCESSOR(exceptionAs)
DEFINE_OPERATOR_ACCESSOR(exceptionAsAny)
DEFINE_OPERATOR_ACCESSOR(continueException)
DEFINE_OPERATOR_ACCESSOR(unhandledExceptionInExternal)
DEFINE_OPERATOR_ACCESSOR(exceptionInInitializer)
DEFINE_OPERATOR_ACCESSOR(exceptionInFinalizer)
DEFINE_OPERATOR_ACCESSOR(packMultiValuedFreeVarByRef)
DEFINE_OPERATOR_ACCESSOR(packMultiValuedFreeVar)
DEFINE_OPERATOR_ACCESSOR(unpackMultiValuedFreeVarAndDereference)
DEFINE_OPERATOR_ACCESSOR(unpackMultiValuedFreeVar)
DEFINE_OPERATOR_ACCESSOR(variantReprType)
DEFINE_OPERATOR_ACCESSOR(DispatchTagCount);
DEFINE_OPERATOR_ACCESSOR(dispatchTag)
DEFINE_OPERATOR_ACCESSOR(dispatchIndex)
DEFINE_OPERATOR_ACCESSOR(invalidDispatch)
DEFINE_OPERATOR_ACCESSOR(stringLiteral)
DEFINE_OPERATOR_ACCESSOR(ifExpression)
DEFINE_OPERATOR_ACCESSOR(typeToRValue)
DEFINE_OPERATOR_ACCESSOR(typesToRValues)
DEFINE_OPERATOR_ACCESSOR(doIntegerAddChecked);
DEFINE_OPERATOR_ACCESSOR(doIntegerSubtractChecked);
DEFINE_OPERATOR_ACCESSOR(doIntegerMultiplyChecked);
DEFINE_OPERATOR_ACCESSOR(dointegerQuotientChecked);
DEFINE_OPERATOR_ACCESSOR(doIntegerRemainderChecked);
DEFINE_OPERATOR_ACCESSOR(doIntegerShiftLeftChecked);
DEFINE_OPERATOR_ACCESSOR(doIntegerNegateChecked);
DEFINE_OPERATOR_ACCESSOR(doIntegerConvertChecked);

}
