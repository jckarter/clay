#include "clay.hpp"
#include "loader.hpp"
#include "patterns.hpp"
#include "analyzer.hpp"
#include "codegen.hpp"
#include "evaluator.hpp"
#include "constructors.hpp"


#pragma clang diagnostic ignored "-Wcovered-switch-default"


namespace clay {

using namespace std;

static vector<PathString> searchPath;
static vector<llvm::SmallString<32> > moduleSuffixes;

llvm::StringMap<ModulePtr> globalModules;
llvm::StringMap<string> globalFlags;
ModulePtr globalMainModule;


//
// initModuleSuffixes
//

static llvm::StringRef getOS(llvm::Triple const &triple) {
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
    default : {
        llvm::StringRef os = triple.getOSName();
        if (os.empty())
            error("cannot initialize os name");
        return os;
    }
    }
}

static llvm::StringRef getOSGroup(llvm::Triple const &triple) {
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

static llvm::StringRef getCPU(llvm::Triple const &triple) {
    switch (triple.getArch()) {
    case llvm::Triple::arm :
    case llvm::Triple::thumb : return "arm";
    case llvm::Triple::ppc :
    case llvm::Triple::ppc64 : return "ppc";
    case llvm::Triple::sparc :
    case llvm::Triple::sparcv9 : return "sparc";
    case llvm::Triple::x86 :
    case llvm::Triple::x86_64 : return "x86";
    default : {
        llvm::StringRef archName = triple.getArchName();
        if (archName.empty())
            error("cannot initialize cpu name");
        return archName;
    }
    }
}

static llvm::StringRef getPtrSize(const llvm::TargetData *targetData) {
    switch (targetData->getPointerSizeInBits()) {
    case 16 : return "16";
    case 32 : return "32";
    case 64 : return "64";
    default : error("cannot initialize ptr size"); return "";
    }
}

static void initModuleSuffixes() {
    llvm::Triple triple(llvmModule->getTargetTriple());

    llvm::StringRef os = getOS(triple);
    llvm::StringRef osgroup = getOSGroup(triple);
    llvm::StringRef cpu = getCPU(triple);
    llvm::StringRef bits = getPtrSize(llvmTargetData);

    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream sout(buf);

#define ADD_SUFFIX(fmt) \
    sout << fmt; \
    moduleSuffixes.push_back(sout.str()); \
    buf.clear(); sout.resync();

    ADD_SUFFIX("." << os << "." << cpu << "." << bits << ".clay")
    ADD_SUFFIX("." << os << "." << cpu << ".clay")
    ADD_SUFFIX("." << os << "." << bits << ".clay")
    ADD_SUFFIX("." << cpu << "." << bits << ".clay")
    ADD_SUFFIX("." << os << ".clay")
    ADD_SUFFIX("." << cpu << ".clay")
    ADD_SUFFIX("." << bits << ".clay")
    if (!osgroup.empty()) {
        ADD_SUFFIX("." << osgroup << ".clay")
    }

#undef ADD_SUFFIX

    moduleSuffixes.push_back(llvm::StringRef(".clay"));
}

void initLoader() {
    initModuleSuffixes();
}


//
// toKey
//

static string toKey(DottedNamePtr name) {
    string key;
    for (unsigned i = 0; i < name->parts.size(); ++i) {
        if (i != 0)
            key.push_back('.');
        key.append(name->parts[i]->str.begin(), name->parts[i]->str.end());
    }
    return key;
}



//
// addSearchPath, locateFile, toRelativePath,
// locateModule
//

void setSearchPath(llvm::ArrayRef<PathString>  path) {
    searchPath = path;
}

static bool locateFile(llvm::StringRef relativePath, PathString &path) {
    // relativePath has no suffix
    for (unsigned i = 0; i < searchPath.size(); ++i) {
        PathString pathWOSuffix(searchPath[i]);
        llvm::sys::path::append(pathWOSuffix, relativePath);
        for (unsigned j = 0; j < moduleSuffixes.size(); ++j) {
            path = pathWOSuffix;
            path.append(moduleSuffixes[j].begin(), moduleSuffixes[j].end());
            if (llvm::sys::fs::exists(path.str()))
                return true;
        }
    }
    return false;
}

template<typename Iterator>
static PathString toRelativePathUpto(DottedNamePtr name, Iterator limit) {
    PathString relativePath;
    for (Iterator i = name->parts.begin(); i < limit; ++i)
        llvm::sys::path::append(relativePath, (*i)->str.str());
    llvm::sys::path::append(relativePath, name->parts.back()->str.str());
    // relative path has no suffix
    return relativePath;
}

// foo.bar -> foo/bar/bar
static PathString toRelativePath1(DottedNamePtr name) {
    return toRelativePathUpto(name, name->parts.end());
}

// foo.bar -> foo/bar
static PathString toRelativePath2(DottedNamePtr name) {
    return toRelativePathUpto(name, name->parts.end() - 1);
}

static PathString locateModule(DottedNamePtr name) {
    PathString path;

    PathString relativePath1 = toRelativePath1(name);
    if (locateFile(relativePath1, path))
        return path;

    PathString relativePath2 = toRelativePath2(name);
    if (locateFile(relativePath2, path))
        return path;

    string s;
    llvm::raw_string_ostream ss(s);

    ss << "module not found: " + toKey(name) << "\n";

    ss << "tried relative paths:\n";
    ss << "    " << relativePath1 << "\n";
    ss << "    " << relativePath2 << "\n";

    ss << "with suffixes:\n";
    for (vector<llvm::SmallString<32> >::const_iterator entry = moduleSuffixes.begin();
            entry != moduleSuffixes.end(); ++entry)
    {
        ss << "    " << *entry << "\n";
    }

    ss << "in search path:\n";
    for (vector<PathString>::const_iterator entry = searchPath.begin();
            entry != searchPath.end(); ++entry)
    {
        ss << "    " << *entry << "\n";
    }

    ss.flush();

    error(name, s);
    llvm_unreachable("module not found");
}



//
// loadFile
//

static SourcePtr loadFile(llvm::StringRef fileName, vector<string> *sourceFiles) {
    if (sourceFiles != NULL)
        sourceFiles->push_back(fileName);

    SourcePtr src = new Source(fileName);
    if (llvmDIBuilder != NULL) {
        PathString absFileName(fileName);
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

static void loadDependents(ModulePtr m, vector<string> *sourceFiles, bool verbose);
void initModule(ModulePtr m);

static ModulePtr makePrimitivesModule();
static ModulePtr makeOperatorsModule();
static ModulePtr makeIntrinsicsModule();

static void installGlobals(ModulePtr m) {
    m->env = new Env(m);
    vector<TopLevelItemPtr>::iterator i, end;
    for (i = m->topLevelItems.begin(), end = m->topLevelItems.end();
         i != end; ++i) {
        TopLevelItem *x = i->ptr();
        x->env = m->env;
        switch (x->objKind) {
        case ENUM_DECL : {
            EnumDecl *enumer = (EnumDecl *)x;
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
        case NEW_TYPE_DECL : {
            NewTypeDecl *nt = (NewTypeDecl *)x;
            TypePtr t = newType(nt);
            addGlobal(m, nt->name, nt->visibility, t.ptr());
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

static ModulePtr loadModuleByName(DottedNamePtr name, vector<string> *sourceFiles, bool verbose) {
    string key = toKey(name);

    llvm::StringMap<ModulePtr>::iterator i = globalModules.find(key);
    if (i != globalModules.end())
        return i->second;

    ModulePtr module;

    if (key == "__primitives__") {
        module = makePrimitivesModule();
    } else if (key == "__operators__") {
        module = makeOperatorsModule();
    } else if (key == "__intrinsics__") {
        module = makeIntrinsicsModule();
    }
    else {
        PathString path = locateModule(name);
        if (verbose) {
            llvm::errs() << "loading module " << name->join() << " from " << path << "\n";
        }
        module = parse(key, loadFile(path, sourceFiles));
    }

    globalModules[key] = module;
    loadDependents(module, sourceFiles, verbose);
    installGlobals(module);

    return module;
}

void loadDependent(ModulePtr m, vector<string> *sourceFiles, ImportPtr dependent, bool verbose) {
    ImportPtr x = dependent;
    x->module = loadModuleByName(x->dottedName, sourceFiles, verbose);
    switch (x->importKind) {
    case IMPORT_MODULE : {
            ImportModule *im = (ImportModule *)x.ptr();
            IdentifierPtr name = NULL;
            if (im->alias.ptr()) {
                name = im->alias;
                m->importedModuleNames[im->alias->str].module = x->module;
            } else {
                llvm::ArrayRef<IdentifierPtr> parts = im->dottedName->parts;
                if (parts.size() == 1)
                    name = parts[0];
                else if (x->visibility == PUBLIC)
                    error(x->location,
                          "public imports of dotted module paths must have an \"as <name>\" alias");
                llvm::StringMap<ModuleLookup> *node = &m->importedModuleNames;
                for (size_t i = parts.size() - 1; i >= 1; --i) {
                    node = &(*node)[parts[i]->str].parents;
                }
                (*node)[parts[0]->str].module = x->module;
            }
            
            if (name.ptr()) {
                string nameStr(name->str.begin(), name->str.end());
                if (m->importedNames.count(nameStr))
                    error(name, "name imported already: " + nameStr);
                m->importedNames.insert(nameStr);
                m->allSymbols[nameStr].insert(x->module.ptr());
                if (x->visibility == PUBLIC)
                    m->publicSymbols[nameStr].insert(x->module.ptr());
            }
            
            break;
        }
    case IMPORT_STAR :
        break;
    case IMPORT_MEMBERS : {
            ImportMembers *y = (ImportMembers *)x.ptr();
            for (unsigned i = 0; i < y->members.size(); ++i) {
                ImportedMember &z = y->members[i];
                IdentifierPtr alias = z.alias.ptr() ? z.alias : z.name;
                string aliasStr(alias->str.begin(), alias->str.end());
                if (m->importedNames.count(aliasStr))
                    error(alias, "name imported already: " + aliasStr);
                assert(y->aliasMap.count(aliasStr) == 0);
                m->importedNames.insert(aliasStr);
                y->aliasMap[aliasStr] = z.name;
            }
            break;
        }
    default :
            assert(false);
}
}

static void loadDependents(ModulePtr m, vector<string> *sourceFiles, bool verbose) {
    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii) {
        loadDependent(m, sourceFiles, *ii, verbose);
    }
}

static ModulePtr loadPrelude(vector<string> *sourceFiles, bool verbose) {
    DottedNamePtr dottedName = new DottedName();
    dottedName->parts.push_back(Identifier::get("prelude"));
    return loadModuleByName(dottedName, sourceFiles, verbose);
}

ModulePtr loadProgram(llvm::StringRef fileName, vector<string> *sourceFiles, bool verbose) {
    globalMainModule = parse("", loadFile(fileName, sourceFiles));
    ModulePtr prelude = loadPrelude(sourceFiles, verbose);
    loadDependents(globalMainModule, sourceFiles, verbose);
    installGlobals(globalMainModule);
    initModule(prelude);
    initModule(globalMainModule);
    return globalMainModule;
}

ModulePtr loadProgramSource(llvm::StringRef name, llvm::StringRef source, bool verbose) {
    SourcePtr mainSource = new Source(name,
        llvm::MemoryBuffer::getMemBufferCopy(source));
    if (llvmDIBuilder != NULL) {
        mainSource->debugInfo = (llvm::MDNode*)llvmDIBuilder->createFile(
            "-e",
            "");
    }

    globalMainModule = parse("", mainSource);
    // Don't keep track of source files for -e script
    ModulePtr prelude = loadPrelude(NULL, verbose);
    loadDependents(globalMainModule, NULL, verbose);
    installGlobals(globalMainModule);
    initModule(prelude);
    initModule(globalMainModule);
    return globalMainModule;
}

ModulePtr loadedModule(llvm::StringRef module) {
    if (!globalModules.count(module))
        error("module not loaded: " + module);
    return globalModules[module];
}



//
// initOverload, initVariantInstance, initModule
//

static EnvPtr overloadPatternEnv(OverloadPtr x) {
    EnvPtr env = new Env(x->env);
    llvm::ArrayRef<PatternVar> pvars = x->code->patternVars;
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
    llvm::ArrayRef<FormalArgPtr> formalArgs, bool hasVarArg)
{
    if (mono.monoState == Procedure_NoOverloads && !hasVarArg)
    {
        assert(mono.monoTypes.empty());
        mono.monoState = Procedure_MonoOverload;
        for (size_t i = 0; i < formalArgs.size(); ++i) {
            if (formalArgs[i]->type == NULL)
                goto poly;
            LocationContext loc(formalArgs[i]->type->location);
            PatternPtr argPattern =
                evaluateOnePattern(formalArgs[i]->type, env);
            ObjectPtr argTypeObj = derefDeep(argPattern);
            if (argTypeObj == NULL)
                goto poly;
            TypePtr argType;
            if (!staticToType(argTypeObj, argType))
                error(formalArgs[i], "expecting a type");

            mono.monoTypes.push_back(argType);
        }
    } else
        goto poly;
    return;

poly:
    mono.monoTypes.clear();
    mono.monoState = Procedure_PolyOverload;
    return;
}

static size_t maxParamCount(const CodePtr& code) {
    if (code->hasVarArg)
        return std::numeric_limits<size_t>::max();
    else
        return code->formalArgs.size();
}

static size_t minParamCount(const CodePtr& code) {
    if (code->hasVarArg)
        return code->formalArgs.size() - 1;
    else
        return code->formalArgs.size();
}

static string paramCountString(const CodePtr& code) {
    string s;
    llvm::raw_string_ostream ss(s);
    if (code->hasVarArg) {
        ss << (code->formalArgs.size() - 1) << "+";
    } else {
        ss << code->formalArgs.size();
    }
    ss.flush();
    return s;
}

template <typename T>
static string toString(const T& t) {
    string s;
    llvm::raw_string_ostream ss(s);
    ss << t;
    ss.flush();
    return s;
}

void addProcedureOverload(ProcedurePtr proc, EnvPtr env, OverloadPtr x) {
    if (!!proc->singleOverload && proc->singleOverload != x) {
        // TODO: points to wrong line
        error(x->location, "standalone functions cannot be overloaded");
    }

    if (proc->privateOverload) {
        if (proc->module != x->module) {
            error("symbol " + toString(proc->name->str) + " is declared as private for overload");
        }
    }

    if (proc->interface != NULL) {
        if (maxParamCount(proc->interface->code) < minParamCount(x->code)) {
            error(x->location,
                    "overload has more parameters (" + paramCountString(x->code) +  ")"
                    " than declaration (" + paramCountString(proc->interface->code) + ")");
        }

        if (maxParamCount(x->code) < minParamCount(proc->interface->code)) {
            error(x->location,
                    "overload has fewer parameters (" + paramCountString(x->code) +  ")"
                    " than declaration (" + paramCountString(proc->interface->code) + ")");
        }

        if (proc->interface->code->returnSpecsDeclared && x->code->returnSpecsDeclared) {
            if (x->code->returnSpecs.size() != proc->interface->code->returnSpecs.size()) {
                error(x->location, string() +
                        "overload return count (" + toString(x->code->returnSpecs.size())  + ")"
                        " must be equal to define return count (" + toString(proc->interface->code->returnSpecs.size()) + ")");
            }
        }
    }

    proc->overloads.insert(proc->overloads.begin(), x);
    getProcedureMonoTypes(proc->mono, env,
        x->code->formalArgs,
        x->code->hasVarArg);
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
            addProcedureOverload(proc, env, x);
            break;
        }
        case RECORD_DECL : {
            RecordDecl *r = (RecordDecl *)obj.ptr();
            r->overloads.insert(r->overloads.begin(), x);
            break;
        }
        case VARIANT_DECL : {
            VariantDecl *v = (VariantDecl *)obj.ptr();
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
        case INTRINSIC : {
            error(x->target, "invalid overload target");
        }
        default : {
            error(x->target, "invalid overload target");
        }
        }
    }
}

static void initVariantInstance(InstanceDeclPtr x) {
    EnvPtr env = new Env(x->env);
    llvm::ArrayRef<PatternVar> pvars = x->patternVars;
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
        if (!vt->variant->open)
            error(x->target, "cannot add instances to closed variant");
        vt->variant->instances.push_back(x);
    }
    else {
        if (pattern->kind != PATTERN_STRUCT)
            error(x->target, "not a variant type");
        PatternStruct *ps = (PatternStruct *)pattern.ptr();
        if ((!ps->head) || (ps->head->objKind != VARIANT_DECL))
            error(x->target, "not a variant type");
        VariantDecl *v = (VariantDecl *)ps->head.ptr();
        v->instances.push_back(x);
    }
}

static void checkStaticAssert(StaticAssertTopLevelPtr a) {
    evaluateStaticAssert(a->location, a->cond, a->message, a->env);
}

static void circularImportsError(llvm::ArrayRef<string>  modules) {
    string s;
    llvm::raw_string_ostream ss(s);
    ss << "import loop:\n";
    for (string const *it = modules.begin(); it != modules.end(); ++it) {
        ss << "    " << *it;
        if (it + 1 != modules.end()) {
            // because error() function adds trailing newline
            ss << "\n";
        }
    }
    return error(ss.str());
}

static void initModule(ModulePtr m, llvm::ArrayRef<string>  importChain) {
    if (m->initState == Module::DONE) return;

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


    if (m->initState == Module::RUNNING && !importChain.empty()) {
        // allow prelude to import self
        if (importChain.back() == m->moduleName) {
            return;
        }
    }

    vector<string> importChainNext = importChain;
    importChainNext.push_back(m->moduleName);

    if (m->initState == Module::RUNNING) {
        circularImportsError(importChainNext);
    }

    m->initState = Module::RUNNING;

    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii)
        initModule((*ii)->module, importChainNext);

    m->initState = Module::DONE;

    verifyAttributes(m);

    llvm::ArrayRef<TopLevelItemPtr> items = m->topLevelItems;
    TopLevelItemPtr const *ti, *tend;
    for (ti = items.begin(), tend = items.end(); ti != tend; ++ti) {
        Object *obj = ti->ptr();
        switch (obj->objKind) {
        case OVERLOAD :
            initOverload((Overload *)obj);
            break;
        case INSTANCE_DECL :
            initVariantInstance((InstanceDecl *)obj);
            break;
        case STATIC_ASSERT_TOP_LEVEL:
            checkStaticAssert((StaticAssertTopLevel *)obj);
            break;
        default:
            break;
        }
    }

    if (llvmDIBuilder != NULL) {
        llvm::DIFile file = m->location.ok()
            ? m->location.source->getDebugInfo()
            : llvm::DIFile(NULL);
        m->debugInfo = (llvm::MDNode*)llvmDIBuilder->createNameSpace(
            llvm::DICompileUnit(llvmDIBuilder->getCU()), // scope
            m->moduleName, // name
            file, // file
            1 // line
            );
    }
}

void initModule(ModulePtr m) {
    initModule(m, vector<string>());
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
    case NEW_TYPE : {
        NewType *et = (NewType *)t.ptr();
        return envModule(et->newtype->env);
    }
    default :
        assert(false);
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
    case MODULE : {
        return (Module*)x.ptr();
    }
    case INTRINSIC : {
        return intrinsicsModule();
    }
    case GLOBAL_VARIABLE :
    case GLOBAL_ALIAS :
    case EXTERNAL_PROCEDURE :
    case EXTERNAL_VARIABLE :
    case PROCEDURE :
    case RECORD_DECL :
    case VARIANT_DECL : {
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

llvm::StringRef primOpName(const PrimOpPtr& x) {
    map<int, string>::iterator i = primOpNames.find(x->primOpCode);
    assert(i != primOpNames.end());
    return i->second;
}



//
// makePrimitivesModule
//

static string toPrimStr(llvm::StringRef s) {
    string name = s;
    if (name[s.size() - 1] == 'P')
        name[s.size() - 1] = '?';
    return name;
}

static void addPrim(ModulePtr m, llvm::StringRef name, ObjectPtr x) {
    m->globals[name] = x;
    m->allSymbols[name].insert(x);
    m->publicGlobals[name] = x;
    m->publicSymbols[name].insert(x);
}

static void addPrimOp(ModulePtr m, llvm::StringRef name, PrimOpPtr x) {
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
        new GlobalAlias(prims.ptr(),
                        Identifier::get("ExceptionsEnabled?"),
                        PUBLIC,
                        vector<PatternVar>(),
                        NULL,
                        vector<IdentifierPtr>(),
                        NULL,
                        new BoolLiteral(exceptionsEnabled()));
    addPrim(prims, "ExceptionsEnabled?", v.ptr());

    vector<IdentifierPtr> recordParams;
    RecordBodyPtr recordBody = new RecordBody(vector<RecordFieldPtr>());
    recordParams.push_back(Identifier::get("T"));
    RecordDeclPtr byRefRecord = new RecordDecl(
        prims.ptr(),
        Identifier::get("ByRef"),
        PUBLIC,
        vector<PatternVar>(),
        NULL,
        recordParams,
        NULL,
        recordBody);
    byRefRecord->env = new Env(prims);
    addPrim(prims, "ByRef", byRefRecord.ptr());

    recordParams.clear();
    recordParams.push_back(Identifier::get("Properties"));
    recordParams.push_back(Identifier::get("Fields"));
    RecordDeclPtr rwpRecord = new RecordDecl(
        prims.ptr(),
        Identifier::get("RecordWithProperties"),
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

    PRIMITIVE(SymbolP);
    
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

    PRIMITIVE(BaseType);

    PRIMITIVE(Static);
    PRIMITIVE(StaticName);
    PRIMITIVE(staticIntegers);
    PRIMITIVE(integers);
    PRIMITIVE(staticFieldRef);

    PRIMITIVE(MainModule);
    PRIMITIVE(StaticModule);
    PRIMITIVE(ModuleName);
    PRIMITIVE(ModuleMemberNames);

    PRIMITIVE(EnumP);
    PRIMITIVE(EnumMemberCount);
    PRIMITIVE(EnumMemberName);
    PRIMITIVE(enumToInt);
    PRIMITIVE(intToEnum);

    PRIMITIVE(StringLiteralP);
    PRIMITIVE(stringLiteralByteIndex);
    PRIMITIVE(stringLiteralBytes);
    PRIMITIVE(stringLiteralByteSize);
    PRIMITIVE(stringLiteralByteSlice);
    PRIMITIVE(stringLiteralConcat);
    PRIMITIVE(stringLiteralFromBytes);

    PRIMITIVE(stringTableConstant);

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

static void addOperator(ModulePtr module, llvm::StringRef name) {
    IdentifierPtr ident = Identifier::get(name);
    ProcedurePtr opProc = new Procedure(module.ptr(), ident, PUBLIC, false);
    opProc->env = new Env(module);
    addGlobal(module, ident, PUBLIC, opProc.ptr());
}

static ModulePtr makeOperatorsModule() {
    ModulePtr operators = new Module("__operators__");

#define OPERATOR(x) addOperator(operators, toPrimStr(#x))

    OPERATOR(dereference);
    OPERATOR(prefixOperator);
    OPERATOR(infixOperator);
    OPERATOR(caseP);
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
    OPERATOR(prefixUpdateAssign);
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
    OPERATOR(nextValue);
    OPERATOR(hasValueP);
    OPERATOR(getValue);
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
    OPERATOR(doIntegerQuotientChecked);
    OPERATOR(doIntegerRemainderChecked);
    OPERATOR(doIntegerShiftLeftChecked);
    OPERATOR(doIntegerNegateChecked);
    OPERATOR(doIntegerConvertChecked);

#undef OPERATOR

    return operators;
}

ModulePtr makeIntrinsicsModule() {
    ModulePtr intrinsics = new Module("__intrinsics__");
    intrinsics->isIntrinsicsModule = true;
    return intrinsics;
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

ModulePtr intrinsicsModule() {
    static ModulePtr cached;
    if (!cached)
        cached = loadedModule("__intrinsics__");
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

static IdentifierPtr fnameToIdent(llvm::StringRef str) {
    string s = str;
    if ((s.size() > 0) && (s[s.size()-1] == 'P'))
        s[s.size()-1] = '?';
    return Identifier::get(s);
}

static ObjectPtr convertObject(ObjectPtr x) {
    switch (x->objKind) {
    case RECORD_DECL : {
        RecordDecl *y = (RecordDecl *)x.ptr();
        if (y->params.empty() && !y->varParam.ptr())
            return recordType(y, vector<ObjectPtr>()).ptr();
        return x;
    }
    case VARIANT_DECL : {
        VariantDecl *y = (VariantDecl *)x.ptr();
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
DEFINE_PRIMITIVE_ACCESSOR(AttributeCCall)
DEFINE_PRIMITIVE_ACCESSOR(AttributeStdCall)
DEFINE_PRIMITIVE_ACCESSOR(AttributeFastCall)
DEFINE_PRIMITIVE_ACCESSOR(AttributeThisCall)
DEFINE_PRIMITIVE_ACCESSOR(AttributeLLVMCall)
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
DEFINE_OPERATOR_ACCESSOR(prefixOperator)
DEFINE_OPERATOR_ACCESSOR(infixOperator)
DEFINE_OPERATOR_ACCESSOR(caseP)
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
DEFINE_OPERATOR_ACCESSOR(prefixUpdateAssign)
DEFINE_OPERATOR_ACCESSOR(indexAssign)
DEFINE_OPERATOR_ACCESSOR(indexUpdateAssign)
DEFINE_OPERATOR_ACCESSOR(fieldRefAssign)
DEFINE_OPERATOR_ACCESSOR(fieldRefUpdateAssign)
DEFINE_OPERATOR_ACCESSOR(staticIndexAssign)
DEFINE_OPERATOR_ACCESSOR(staticIndexUpdateAssign)
DEFINE_OPERATOR_ACCESSOR(callMain)
DEFINE_OPERATOR_ACCESSOR(charLiteral)
DEFINE_OPERATOR_ACCESSOR(iterator)
DEFINE_OPERATOR_ACCESSOR(nextValue)
DEFINE_OPERATOR_ACCESSOR(hasValueP)
DEFINE_OPERATOR_ACCESSOR(getValue)
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
DEFINE_OPERATOR_ACCESSOR(DispatchTagCount)
DEFINE_OPERATOR_ACCESSOR(dispatchTag)
DEFINE_OPERATOR_ACCESSOR(dispatchIndex)
DEFINE_OPERATOR_ACCESSOR(invalidDispatch)
DEFINE_OPERATOR_ACCESSOR(stringLiteral)
DEFINE_OPERATOR_ACCESSOR(ifExpression)
DEFINE_OPERATOR_ACCESSOR(typeToRValue)
DEFINE_OPERATOR_ACCESSOR(typesToRValues)
DEFINE_OPERATOR_ACCESSOR(doIntegerAddChecked)
DEFINE_OPERATOR_ACCESSOR(doIntegerSubtractChecked)
DEFINE_OPERATOR_ACCESSOR(doIntegerMultiplyChecked)
DEFINE_OPERATOR_ACCESSOR(doIntegerQuotientChecked)
DEFINE_OPERATOR_ACCESSOR(doIntegerRemainderChecked)
DEFINE_OPERATOR_ACCESSOR(doIntegerShiftLeftChecked)
DEFINE_OPERATOR_ACCESSOR(doIntegerNegateChecked)
DEFINE_OPERATOR_ACCESSOR(doIntegerConvertChecked)

void addGlobals(ModulePtr m, llvm::ArrayRef<TopLevelItemPtr>  toplevels) {
    TopLevelItemPtr const *i, *end;
    for (i = toplevels.begin(), end = toplevels.end();
    i != end; ++i) {
        m->topLevelItems.push_back(*i);
        TopLevelItem *x = i->ptr();
        x->env = m->env;
        switch (x->objKind) {
        case ENUM_DECL : {
                EnumDecl *enumer = (EnumDecl *)x;
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

    llvm::ArrayRef<TopLevelItemPtr> items = m->topLevelItems;
    for (size_t i = items.size() - toplevels.size(); i < items.size(); ++i) {
        Object *obj = items[i].ptr();
        switch (obj->objKind) {
        case OVERLOAD :
            initOverload((Overload *)obj);
            break;
        case INSTANCE_DECL :
            initVariantInstance((InstanceDecl *)obj);
            break;
        case STATIC_ASSERT_TOP_LEVEL:
            checkStaticAssert((StaticAssertTopLevel *)obj);
            break;
        default:
            break;
        }
    }

}

}
