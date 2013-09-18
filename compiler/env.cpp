#include "env.hpp"
#include "clay.hpp"
#include "loader.hpp"
#include "error.hpp"


namespace clay {

using namespace std;

typedef llvm::StringMap<ObjectPtr>::iterator MapIter;



//
// addGlobal
//

void addGlobal(ModulePtr module,
               IdentifierPtr name,
               Visibility visibility,
               ObjectPtr value)
{
    MapIter i = module->globals.find(name->str);
    if (i != module->globals.end())
        error(name, "name redefined: " + name->str);
    module->globals[name->str] = value;
    module->allSymbols[name->str].insert(value);
    if (visibility == PUBLIC) {
        module->publicGlobals[name->str] = value;
        module->publicSymbols[name->str].insert(value);
    }
}



//
// module errors
//

static void suggestModules(llvm::raw_ostream &err, set<string> const &moduleNames, llvm::StringRef name) {
    for (set<string>::const_iterator i = moduleNames.begin(), end = moduleNames.end();
         i != end;
         ++i)
    {
        err << "\n    import " << *i << ".(" << name << ");";
    }
}

static void ambiguousImportError(IdentifierPtr name, ImportSet const &candidates) {
    string buf;
    llvm::raw_string_ostream err(buf);
    err << "ambiguous imported symbol: " << name->str;
    set<string> moduleNames;
    for (const ObjectPtr *i = candidates.begin(), *end = candidates.end();
         i != end;
         ++i)
    {
        moduleNames.insert(staticModule(*i)->moduleName);
    }

    err << "\n  disambiguate with one of:";
    suggestModules(err, moduleNames, name->str);
    error(name, err.str());
}

static void undefinedNameError(IdentifierPtr name) {
    string buf;
    llvm::raw_string_ostream err(buf);
    err << "undefined name: " << name->str;
    set<string> suggestModuleNames;

    for (llvm::StringMap<ModulePtr>::const_iterator i = globalModules.begin(), end = globalModules.end();
         i != end;
         ++i)
    {
        if (lookupPublic(i->second, name) != NULL)
            suggestModuleNames.insert(i->second->moduleName);
    }

    if (!suggestModuleNames.empty()) {
        err << "\n  maybe you need one of:";
        suggestModules(err, suggestModuleNames, name->str);
    }

    error(name, err.str());

}


//
// getPublicSymbols, getAllSymbols
//


static const llvm::StringMap<ImportSet> &getPublicSymbols(ModulePtr module);
static const llvm::StringMap<ImportSet> &getAllSymbols(ModulePtr module);

static void addImportedSymbols(ModulePtr module, bool publicOnly);

static void getIntrinsicIdentifier(llvm::SmallVectorImpl<char> *out, llvm::StringRef in)
{
    // + 5 because all intrinsic names start with "llvm."
    for (char const *i = in.begin() + 5, *end = in.end(); i != end; ++i)
        if (*i == '.')
            out->push_back('_');
        else
            out->push_back(*i);
}

static void getIntrinsicSymbols(ModulePtr module)
{
    assert(module->isIntrinsicsModule);
    module->publicSymbols.clear();
    module->allSymbols.clear();
    static const char * const intrinsicNames[] = {
        "",
#define GET_INTRINSIC_NAME_TABLE
#include "llvm/IR/Intrinsics.gen"
#undef GET_INTRINSIC_NAME_TABLE
    };
    for (size_t idIter = llvm::Intrinsic::ID(1);
         idIter < llvm::Intrinsic::num_intrinsics;
         ++idIter)
    {
        llvm::Intrinsic::ID id = static_cast<llvm::Intrinsic::ID>(idIter);
        llvm::SmallString<64> identifier;
        getIntrinsicIdentifier(&identifier, intrinsicNames[id]);
        module->publicSymbols[identifier].insert(
            new IntrinsicSymbol(module.ptr(), Identifier::get(identifier), id));
        module->allSymbols[identifier].insert(
            new IntrinsicSymbol(module.ptr(), Identifier::get(identifier), id));
    }
    module->publicSymbolsLoaded = 1;
    module->allSymbolsLoaded = 1;
}

static const llvm::StringMap<ImportSet> &getPublicSymbols(ModulePtr module)
{
    if (module->publicSymbolsLoaded)
        return module->publicSymbols;
    if (module->publicSymbolsLoading >= 1)
        return module->publicSymbols;
    if (module->isIntrinsicsModule) {
        getIntrinsicSymbols(module);
        return module->publicSymbols;
    }
    module->publicSymbolsLoading += 1;
    addImportedSymbols(module, /*publicOnly=*/ true);
    module->publicSymbolsLoading -= 1;
    return module->publicSymbols;
}

static const llvm::StringMap<ImportSet> &getAllSymbols(ModulePtr module)
{
    if (module->allSymbolsLoaded)
        return module->allSymbols;
    if (module->allSymbolsLoading >= 1)
        return module->allSymbols;
    if (module->isIntrinsicsModule) {
        getIntrinsicSymbols(module);
        return module->allSymbols;
    }
    module->allSymbolsLoading += 1;

    addImportedSymbols(module, /*publicOnly=*/ false);

    module->allSymbolsLoading -= 1;
    return module->allSymbols;
}

static void insertImported(IdentifierPtr name,
                           ObjectPtr value,
                           const llvm::StringMap<ObjectPtr> &globals,
                           llvm::StringMap<ImportSet> &result,
                           set<string> &specificImported,
                           bool isSpecificImport)
{
    string nameStr(name->str.begin(), name->str.end());
    if (specificImported.count(nameStr)) {
        if (isSpecificImport)
            error(name, "name imported already: " + name->str);
        return;
    }

    if (!globals.count(nameStr)) {
        if (isSpecificImport) {
            result[nameStr].clear();
            specificImported.insert(nameStr);
        }
        result[nameStr].insert(value);
    }
}


static void addImportedSymbols(ModulePtr module,
                               bool publicOnly)
{
    llvm::StringMap<ImportSet> &result =
        publicOnly ? module->publicSymbols : module->allSymbols;
    set<string> specificImported;

    const llvm::StringMap<ObjectPtr> &globals = module->globals;

    vector<ImportPtr>::iterator
        ii = module->imports.begin(),
        iend = module->imports.end();
    for (; ii != iend; ++ii) {
        Import *x = ii->ptr();
        if (publicOnly && (x->visibility != PUBLIC))
            continue;
        if (x->importKind == IMPORT_MODULE) {
            ImportModule *y = (ImportModule *)x;
            IdentifierPtr name = NULL;
            if (y->alias.ptr()) {
                name = y->alias;
            }
            else {
                if (y->dottedName->parts.size() == 1)
                    name = y->dottedName->parts[0];
            }
            if (name.ptr()) {
                // Module imports are set up by loadDependents, so don't alter them here;
                // only add them to specificImported so they catch conflicts with other imports
                string nameStr(name->str.begin(), name->str.end());
                if (specificImported.count(nameStr)) {
                    error(name, "name imported already: " + nameStr);
                }
                specificImported.insert(nameStr);
            }
        }
        else if (x->importKind == IMPORT_STAR) {
            ImportStar *y = (ImportStar *)x;
            const llvm::StringMap<ImportSet> &symbols2 =
                getPublicSymbols(y->module);
            llvm::StringMap<ImportSet>::const_iterator
                mmi = symbols2.begin(),
                mmend = symbols2.end();
            for (; mmi != mmend; ++mmi) {
                const ObjectPtr *oi = mmi->second.begin(), *oend = mmi->second.end();
                for (; oi != oend; ++oi) {
                    IdentifierPtr fakeIdent = Identifier::get(mmi->getKey(), y->location);
                    insertImported(fakeIdent, *oi, globals, result, specificImported, false);
                }
            }
        }
        else if (x->importKind == IMPORT_MEMBERS) {
            ImportMembers *y = (ImportMembers *)x;
            const llvm::StringMap<ImportSet> &publicSymbols =
                getPublicSymbols(y->module);
            const llvm::StringMap<ImportSet> &allSymbols =
                getAllSymbols(y->module);
            for (size_t i = 0; i < y->members.size(); ++i) {
                const ImportedMember &z = y->members[i];
                const llvm::StringMap<ImportSet> &memberSymbols =
                    z.visibility == PRIVATE ? allSymbols : publicSymbols;
                llvm::StringMap<ImportSet>::const_iterator si =
                    memberSymbols.find(z.name->str);
                if ((si == memberSymbols.end()) || si->second.empty())
                    error(z.name, "imported name not found");
                const ImportSet &objs = si->second;
                IdentifierPtr name = z.name;
                if (z.alias.ptr())
                    name = z.alias;
                const ObjectPtr *oi = objs.begin(), *oend = objs.end();
                for (; oi != oend; ++oi)
                    insertImported(name, *oi, globals, result, specificImported, true);
            }
        }
        else {
            assert(false);
        }
    }
}



//
// lookupPrivate
//

ObjectPtr lookupPrivate(ModulePtr module, IdentifierPtr name) {
retry:
    llvm::StringMap<ImportSet>::const_iterator i =
        module->allSymbols.find(name->str);
    if ((i == module->allSymbols.end()) || (i->second.empty())) {
        if (!module->allSymbolsLoaded) {
            getAllSymbols(module);
            module->allSymbolsLoaded = true;
            goto retry;
        }
        ModulePtr prelude = preludeModule();
        if (module == prelude)
            return NULL;
        else
            return lookupPublic(prelude, name);
    }
    const ImportSet &objs = i->second;
    if (objs.size() > 1) {
        ambiguousImportError(name, objs);
    }
    return *objs.begin();
}



//
// lookupPublic, safeLookupPublic
//

ObjectPtr lookupPublic(ModulePtr module, IdentifierPtr name) {
retry:
    llvm::StringMap<ImportSet>::const_iterator i =
        module->publicSymbols.find(name->str);
    if ((i == module->publicSymbols.end()) || (i->second.empty())) {
        
        if (!module->publicSymbolsLoaded) {
            getPublicSymbols(module);
            module->publicSymbolsLoaded = true;
            goto retry;
        }
        return NULL;
    }
    const ImportSet &objs = i->second;
    if (objs.size() > 1) {
        ambiguousImportError(name, objs);
    }
    return *objs.begin();
}

ObjectPtr safeLookupPublic(ModulePtr module, IdentifierPtr name) {
    ObjectPtr x = lookupPublic(module, name);
    if (!x)
        undefinedNameError(name);
    return x;
}



//
// addLocal, safeLookupEnv
//

void addLocal(EnvPtr env, IdentifierPtr name, ObjectPtr value) {
    MapIter i = env->entries.find(name->str);
    if (i != env->entries.end())
        error(name, "duplicate name: " + name->str);
    env->entries[name->str] = value;
}

ObjectPtr lookupEnv(EnvPtr env, IdentifierPtr name) {
    MapIter i = env->entries.find(name->str);
    if (i != env->entries.end())
        return i->second;
    if (env->parent.ptr()) {
        switch (env->parent->objKind) {
        case ENV : {
            Env *y = (Env *)env->parent.ptr();
            return lookupEnv(y, name);
        }
        case MODULE : {
            Module *y = (Module *)env->parent.ptr();
            ObjectPtr z = lookupPrivate(y, name);
            return z;
        }
        default :
            assert(false);
            return NULL;
        }
    }
    else
        return NULL;
}

ObjectPtr safeLookupEnv(EnvPtr env, IdentifierPtr name) {
    ObjectPtr obj = lookupEnv(env, name);
    if (obj == NULL)
        undefinedNameError(name);
    return obj;
}

ModulePtr safeLookupModule(EnvPtr env) {
    switch (env->parent->objKind) {
    case ENV : {
        Env *parent = (Env *)env->parent.ptr();
        return safeLookupModule(parent);
    }
    case MODULE : {
        Module *module = (Module *)env->parent.ptr();
        return module;
    }
    default :
        assert(false);
        return NULL;
    }
}

llvm::DINameSpace lookupModuleDebugInfo(EnvPtr env) {
    if (env == NULL || env->parent == NULL)
        return llvm::DINameSpace(NULL);

    switch (env->parent->objKind) {
    case ENV : {
        Env *parent = (Env *)env->parent.ptr();
        return lookupModuleDebugInfo(parent);
    }
    case MODULE : {
        Module *module = (Module *)env->parent.ptr();
        return module->getDebugInfo();
    }
    default :
        return llvm::DINameSpace(NULL);
    }
}


//
// lookupEnvEx
//

ObjectPtr lookupEnvEx(EnvPtr env, IdentifierPtr name,
                      EnvPtr nonLocalEnv, bool &isNonLocal,
                      bool &isGlobal)
{
    if (nonLocalEnv == env)
        nonLocalEnv = NULL;

    MapIter i = env->entries.find(name->str);
    if (i != env->entries.end()) {
        if (!nonLocalEnv)
            isNonLocal = true;
        else
            isNonLocal = false;
        isGlobal = false;
        return i->second;
    }

    if (!env->parent) {
        undefinedNameError(name);
    }

    switch (env->parent->objKind) {
    case ENV : {
        Env *y = (Env *)env->parent.ptr();
        return lookupEnvEx(y, name, nonLocalEnv, isNonLocal, isGlobal);
    }
    case MODULE : {
        Module *y = (Module *)env->parent.ptr();
        ObjectPtr z = lookupPrivate(y, name);
        if (!z)
            undefinedNameError(name);
        isNonLocal = true;
        isGlobal = true;
        return z;
    }
    default :
        assert(false);
        return NULL;
    }
}



//
// foreignExpr
//

ExprPtr foreignExpr(EnvPtr env, ExprPtr expr)
{
    if (expr->exprKind == UNPACK) {
        Unpack *y = (Unpack *)expr.ptr();
        return new Unpack(foreignExpr(env, y->expr));
    }
    return new ForeignExpr(env, expr);
}


//
// lookupCallByNameExprHead
//

ExprPtr lookupCallByNameExprHead(EnvPtr env)
{
    if (env->callByNameExprHead.ptr())
        return env->callByNameExprHead;

    if (env->parent.ptr()) {
        switch (env->parent->objKind) {
        case ENV : {
            Env *p = (Env *)env->parent.ptr();
            return lookupCallByNameExprHead(p);
        }
        default : {
            return ExprPtr();
        }
        }
    }

    assert(false);
    return NULL;
}


//
// safeLookupCallByNameLocation
//

Location safeLookupCallByNameLocation(EnvPtr env)
{
    ExprPtr head = lookupCallByNameExprHead(env);
    if (head.ptr() == 0) {
        error("__FILE__, __LINE__, and __COLUMN__ are only allowed in an alias function");
    }
    return head->location;
}

bool lookupExceptionAvailable(const Env* env) {
    if (env->exceptionAvailable)
        return true;

    if (env->parent.ptr() && env->parent->objKind == ENV)
        return lookupExceptionAvailable((const Env*) env->parent.ptr());

    return false;
}

}
