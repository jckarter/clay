#include "clay.hpp"
#include <cassert>

using namespace std;

typedef map<string, ObjectPtr>::iterator MapIter;



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

ObjectPtr lookupModuleHolder(ModuleHolderPtr mh, IdentifierPtr name) {
    ObjectPtr result1, result2;
    map<string, ModuleHolderPtr>::iterator i = mh->children.find(name->str);
    if (i != mh->children.end())
        result1 = i->second.ptr();
    if (mh->import.ptr())
        result2 = lookupPublic(mh->import->module, name);
    if (result1.ptr()) {
        if (result2.ptr() && (result1 != result2))
            error(name, "ambiguous imported symbol: " + name->str);
        return result1;
    }
    else {
        return result2;
    }
}

ObjectPtr safeLookupModuleHolder(ModuleHolderPtr mh, IdentifierPtr name) {
    ObjectPtr x = lookupModuleHolder(mh, name);
    if (!x)
        error(name, "undefined name: " + name->str);
    return x;
}



//
// getPublicSymbols, getAllSymbols
//


typedef set<ObjectPtr> ObjectSet;
static const map<string, ObjectSet> &getPublicSymbols(ModulePtr module);
static const map<string, ObjectSet> &getAllSymbols(ModulePtr module);

static void addImportedSymbols(ModulePtr module, bool publicOnly);

static const map<string, ObjectSet> &getPublicSymbols(ModulePtr module)
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

static const map<string, ObjectSet> &getAllSymbols(ModulePtr module)
{
    if (module->allSymbolsLoaded)
        return module->allSymbols;
    if (module->allSymbolsLoading >= 1)
        return module->allSymbols;
    module->allSymbolsLoading += 1;

    addImportedSymbols(module, false);

    set<string> definedNames;
    map<string, ObjectSet>::const_iterator
        i = module->allSymbols.begin(),
        end = module->allSymbols.end();
    for (; i != end; ++i) {
        if (!(i->second.empty()))
            definedNames.insert(i->first);
    }

    ModulePtr prelude = loadedModule("prelude");
    const map<string, ObjectSet> &preludeSymbols =
        getPublicSymbols(prelude);
    i = preludeSymbols.begin();
    end = preludeSymbols.end();
    for (; i != end; ++i) {
        const string &name = i->first;
        if (!definedNames.count(name)) {
            const ObjectSet &objs = i->second;
            module->allSymbols[name].insert(objs.begin(), objs.end());
        }
    }

    module->allSymbolsLoading -= 1;
    return module->allSymbols;
}

static void insertImported(const string &name,
                           ObjectPtr value,
                           const map<string, ObjectPtr> &globals,
                           map<string, ObjectSet> &result)
{
    if (!globals.count(name))
        result[name].insert(value);
}

static void addImportedSymbols(ModulePtr module,
                               bool publicOnly)
{
    map<string, ObjectSet> &result =
        publicOnly ? module->publicSymbols : module->allSymbols;
    const map<string, ObjectPtr> &globals = module->globals;

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
                const string &name = y->alias->str;
                ModuleHolderPtr holder =
                    publicOnly ?
                    module->publicRootHolder->children[name] :
                    module->rootHolder->children[name];
                insertImported(name, holder.ptr(), globals, result);
            }
            else {
                const string &name = y->dottedName->parts[0]->str;
                ModuleHolderPtr holder =
                    publicOnly ?
                    module->publicRootHolder->children[name] :
                    module->rootHolder->children[name];
                insertImported(name, holder.ptr(), globals, result);
            }
        }
        else if (x->importKind == IMPORT_STAR) {
            ImportStar *y = (ImportStar *)x;
            const map<string, ObjectSet> &symbols2 =
                getPublicSymbols(y->module);
            map<string, ObjectSet>::const_iterator
                mmi = symbols2.begin(),
                mmend = symbols2.end();
            for (; mmi != mmend; ++mmi) {
                ObjectSet::const_iterator
                    oi = mmi->second.begin(),
                    oend = mmi->second.end();
                for (; oi != oend; ++oi)
                    insertImported(mmi->first, *oi, globals, result);
            }
        }
        else if (x->importKind == IMPORT_MEMBERS) {
            ImportMembers *y = (ImportMembers *)x;
            const map<string, ObjectSet> &symbols2 =
                getPublicSymbols(y->module);
            for (unsigned i = 0; i < y->members.size(); ++i) {
                const ImportedMember &z = y->members[i];
                map<string, ObjectSet>::const_iterator si =
                    symbols2.find(z.name->str);
                if ((si == symbols2.end()) || si->second.empty())
                    error(z.name, "imported name not found");
                const ObjectSet &objs = si->second;
                IdentifierPtr name = z.name;
                if (z.alias.ptr())
                    name = z.alias;
                ObjectSet::const_iterator
                    oi = objs.begin(),
                    oend = objs.end();
                for (; oi != oend; ++oi)
                    insertImported(name->str, *oi, globals, result);
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
        module->allSymbols = getAllSymbols(module);
        module->allSymbolsLoaded = true;
    }
    map<string, ObjectSet>::const_iterator i =
        module->allSymbols.find(name->str);
    if ((i == module->allSymbols.end()) || (i->second.empty()))
        return NULL;
    const ObjectSet &objs = i->second;
    if (objs.size() > 1)
        error(name, "ambiguous imported symbol: " + name->str);
    return *objs.begin();
}



//
// lookupPublic, safeLookupPublic
//

ObjectPtr lookupPublic(ModulePtr module, IdentifierPtr name) {
    if (!module->publicSymbolsLoaded) {
        module->publicSymbols = getPublicSymbols(module);
        module->publicSymbolsLoaded = true;
    }
    map<string, ObjectSet>::const_iterator i =
        module->publicSymbols.find(name->str);
    if ((i == module->publicSymbols.end()) || (i->second.empty()))
        return NULL;
    const ObjectSet &objs = i->second;
    if (objs.size() > 1)
        error(name, "ambiguous imported symbol: " + name->str);
    return *objs.begin();
}

ObjectPtr safeLookupPublic(ModulePtr module, IdentifierPtr name) {
    ObjectPtr x = lookupPublic(module, name);
    if (!x)
        error(name, "undefined name: " + name->str);
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

ObjectPtr safeLookupEnv(EnvPtr env, IdentifierPtr name) {
    MapIter i = env->entries.find(name->str);
    if (i != env->entries.end())
        return i->second;
    if (env->parent.ptr()) {
        switch (env->parent->objKind) {
        case ENV : {
            Env *y = (Env *)env->parent.ptr();
            return safeLookupEnv(y, name);
        }
        case MODULE : {
            Module *y = (Module *)env->parent.ptr();
            ObjectPtr z = lookupPrivate(y, name);
            if (!z)
                error(name, "undefined name: " + name->str);
            return z;
        }
        default :
            assert(false);
            return NULL;
        }
    }
    else {
        error(name, "undefined name: " + name->str);
        return NULL;
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
        error(name, "undefined name: " + name->str);
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
            error(name, "undefined name: " + name->str);
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
