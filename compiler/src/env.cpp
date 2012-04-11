#include "clay.hpp"

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
// lookupModuleHolder, safeLookupModuleHolder
//

typedef set<ObjectPtr> ObjectSet;

static void suggestModules(llvm::raw_ostream &err, set<string> const &moduleNames, IdentifierPtr name) {
    for (set<string>::const_iterator i = moduleNames.begin(), end = moduleNames.end();
         i != end;
         ++i)
    {
        err << "\n    import " << *i << ".(" << name->str << ");";
    }
}

static void ambiguousImportError(IdentifierPtr name, ObjectSet const &candidates) {
    string buf;
    llvm::raw_string_ostream err(buf);
    err << "ambiguous imported symbol: " << name->str;
    set<string> moduleNames;
    for (ObjectSet::const_iterator i = candidates.begin(), end = candidates.end();
         i != end;
         ++i)
    {
        moduleNames.insert(staticModule(*i)->moduleName);
    }

    err << "\n  disambiguate with one of:";
    suggestModules(err, moduleNames, name);
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
        suggestModules(err, suggestModuleNames, name);
    }

    error(name, err.str());

}

ObjectPtr lookupModuleHolder(ModuleHolderPtr mh, IdentifierPtr name) {
    ObjectPtr result1, result2;
    llvm::StringMap<ModuleHolderPtr>::iterator i = mh->children.find(name->str);
    if (i != mh->children.end())
        result1 = i->second.ptr();
    if (mh->module != NULL)
        result2 = lookupPublic(mh->module, name);
    if (result1.ptr()) {
        if (result2.ptr() && (result1 != result2)) {
            ObjectSet candidates;
            candidates.insert(result1);
            candidates.insert(result2);
            ambiguousImportError(name, candidates);
        }
        return result1;
    }
    else {
        return result2;
    }
}

ObjectPtr safeLookupModuleHolder(ModuleHolderPtr mh, IdentifierPtr name) {
    ObjectPtr x = lookupModuleHolder(mh, name);
    if (!x)
        undefinedNameError(name);
    return x;
}



//
// getPublicSymbols, getAllSymbols
//


static const llvm::StringMap<ObjectSet> &getPublicSymbols(ModulePtr module);
static const llvm::StringMap<ObjectSet> &getAllSymbols(ModulePtr module);

static void addImportedSymbols(ModulePtr module, bool publicOnly);

static const llvm::StringMap<ObjectSet> &getPublicSymbols(ModulePtr module)
{
    if (module->publicSymbolsLoaded)
        return module->publicSymbols;
    if (module->publicSymbolsLoading >= 1)
        return module->publicSymbols;
    module->publicSymbolsLoading += 1;
    addImportedSymbols(module, true);
    module->publicSymbolsLoading -= 1;
    return module->publicSymbols;
}

static const llvm::StringMap<ObjectSet> &getAllSymbols(ModulePtr module)
{
    if (module->allSymbolsLoaded)
        return module->allSymbols;
    if (module->allSymbolsLoading >= 1)
        return module->allSymbols;
    module->allSymbolsLoading += 1;

    addImportedSymbols(module, false);

    set<string> definedNames;
    llvm::StringMap<ObjectSet>::const_iterator
        i = module->allSymbols.begin(),
        end = module->allSymbols.end();
    for (; i != end; ++i) {
        if (!(i->second.empty()))
            definedNames.insert(i->getKey());
    }

    ModulePtr prelude = loadedModule("prelude");
    const llvm::StringMap<ObjectSet> &preludeSymbols =
        getPublicSymbols(prelude);
    i = preludeSymbols.begin();
    end = preludeSymbols.end();
    for (; i != end; ++i) {
        llvm::StringRef name = i->getKey();
        if (!definedNames.count(name)) {
            const ObjectSet &objs = i->second;
            module->allSymbols[name].insert(objs.begin(), objs.end());
        }
    }

    module->allSymbolsLoading -= 1;
    return module->allSymbols;
}

static void insertImported(IdentifierPtr name,
                           ObjectPtr value,
                           const llvm::StringMap<ObjectPtr> &globals,
                           llvm::StringMap<ObjectSet> &result,
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
    llvm::StringMap<ObjectSet> &result =
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
            if (y->alias.ptr()) {
                IdentifierPtr name = y->alias;
                ModuleHolderPtr holder =
                    publicOnly ?
                    module->publicRootHolder->children[name->str] :
                    module->rootHolder->children[name->str];
                insertImported(name, holder.ptr(), globals, result, specificImported, true);
            }
            else {
                IdentifierPtr name = y->dottedName->parts[0];
                ModuleHolderPtr holder =
                    publicOnly ?
                    module->publicRootHolder->children[name->str] :
                    module->rootHolder->children[name->str];
                insertImported(name, holder.ptr(), globals, result, specificImported, true);
            }
        }
        else if (x->importKind == IMPORT_STAR) {
            ImportStar *y = (ImportStar *)x;
            const llvm::StringMap<ObjectSet> &symbols2 =
                getPublicSymbols(y->module);
            llvm::StringMap<ObjectSet>::const_iterator
                mmi = symbols2.begin(),
                mmend = symbols2.end();
            for (; mmi != mmend; ++mmi) {
                ObjectSet::const_iterator
                    oi = mmi->second.begin(),
                    oend = mmi->second.end();
                for (; oi != oend; ++oi) {
                    IdentifierPtr fakeIdent = Identifier::get(mmi->getKey(), y->location);
                    insertImported(fakeIdent, *oi, globals, result, specificImported, false);
                }
            }
        }
        else if (x->importKind == IMPORT_MEMBERS) {
            ImportMembers *y = (ImportMembers *)x;
            const llvm::StringMap<ObjectSet> &publicSymbols =
                getPublicSymbols(y->module);
            const llvm::StringMap<ObjectSet> &allSymbols =
                getAllSymbols(y->module);
            for (unsigned i = 0; i < y->members.size(); ++i) {
                const ImportedMember &z = y->members[i];
                const llvm::StringMap<ObjectSet> &memberSymbols =
                    z.visibility == PRIVATE ? allSymbols : publicSymbols;
                llvm::StringMap<ObjectSet>::const_iterator si =
                    memberSymbols.find(z.name->str);
                if ((si == memberSymbols.end()) || si->second.empty())
                    error(z.name, "imported name not found");
                const ObjectSet &objs = si->second;
                IdentifierPtr name = z.name;
                if (z.alias.ptr())
                    name = z.alias;
                ObjectSet::const_iterator
                    oi = objs.begin(),
                    oend = objs.end();
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
    if (!module->allSymbolsLoaded) {
        getAllSymbols(module);
        module->allSymbolsLoaded = true;
    }
    llvm::StringMap<ObjectSet>::const_iterator i =
        module->allSymbols.find(name->str);
    if ((i == module->allSymbols.end()) || (i->second.empty()))
        return NULL;
    const ObjectSet &objs = i->second;
    if (objs.size() > 1) {
        ambiguousImportError(name, objs);
    }
    return *objs.begin();
}



//
// lookupPublic, safeLookupPublic
//

ObjectPtr lookupPublic(ModulePtr module, IdentifierPtr name) {
    if (!module->publicSymbolsLoaded) {
        getPublicSymbols(module);
        module->publicSymbolsLoaded = true;
    }
    llvm::StringMap<ObjectSet>::const_iterator i =
        module->publicSymbols.find(name->str);
    if ((i == module->publicSymbols.end()) || (i->second.empty()))
        return NULL;
    const ObjectSet &objs = i->second;
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
        if (y->expr->exprKind == TUPLE) {
            Tuple *y2 = (Tuple *)y->expr.ptr();
            ExprListPtr out = new ExprList();
            for (unsigned i = 0; i < y2->args->size(); ++i)
                out->add(foreignExpr(env, y2->args->exprs[i]));
            return new Unpack(new Tuple(out));
        }
        else {
            return new Unpack(foreignExpr(env, y->expr));
        }
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

LocationPtr safeLookupCallByNameLocation(EnvPtr env)
{
    ExprPtr head = lookupCallByNameExprHead(env);
    if (head.ptr() == 0) {
        error("__FILE__, __LINE__, and __COLUMN__ are only allowed in an alias function");
        return NULL;
    }
    return head->location;
}

}
