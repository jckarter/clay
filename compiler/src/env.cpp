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
    if (visibility == PUBLIC) {
        module->publicGlobals[name->str] = value;
    }
}

static ObjectPtr
lookupImportedNames(ModulePtr module,
                    IdentifierPtr name,
                    Visibility visibility)
{
    vector<ImportPtr> &x = module->imports;
    vector<ImportPtr>::iterator ii, iend;
    ObjectPtr result;
    for (ii = x.begin(), iend = x.end(); ii != iend; ++ii) {
        ImportPtr y = *ii;
        ObjectPtr entry;
        switch (y->importKind) {
        case IMPORT_MODULE :
            break;
        case IMPORT_STAR : {
            ImportStar *z = (ImportStar *)y.ptr();
            if ((visibility == PRIVATE) || (z->visibility == PUBLIC)) {
                entry = lookupPublic(z->module, name);
            }
            break;
        }
        case IMPORT_MEMBERS : {
            ImportMembers *z = (ImportMembers *)y.ptr();
            if ((visibility == PRIVATE) || (z->visibility == PUBLIC)) {
                IdentifierPtr name2;
                map<string, IdentifierPtr>::iterator i =
                    z->aliasMap.find(name->str);
                if (i != z->aliasMap.end())
                    entry = lookupPublic(z->module, i->second);
            }
            break;
        }
        default :
            assert(false);
            break;
        }
        if (entry.ptr()) {
            if (!result)
                result = entry;
            else if (result != entry)
                error(name, "ambiguous imported symbol: " + name->str);
        }
    }
    return result;
}

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



//
// lookupPrivate
//

ObjectPtr lookupPrivate(ModulePtr module, IdentifierPtr name) {
    if (module->lookupBusy)
        return NULL;
    MapIter i = module->globals.find(name->str);
    if (i != module->globals.end())
        return i->second;
    module->lookupBusy = true;
    ObjectPtr result1 = lookupImportedNames(module, name, PRIVATE);
    ObjectPtr result2 = lookupModuleHolder(module->rootHolder, name);
    module->lookupBusy = false;
    if (result1.ptr()) {
        if (result2.ptr() && (result1 != result2))
            error(name, "ambiguous imported symbol: " + name->str);
        return result1;
    }
    else {
        return result2;
    }
}



//
// lookupPublic
//

ObjectPtr lookupPublic(ModulePtr module, IdentifierPtr name) {
    if (module->lookupBusy)
        return NULL;
    MapIter i = module->publicGlobals.find(name->str);
    if (i != module->publicGlobals.end())
        return i->second;
    module->lookupBusy = true;
    ObjectPtr result1 = lookupImportedNames(module, name, PUBLIC);
    ObjectPtr result2 = lookupModuleHolder(module->publicRootHolder, name);
    module->lookupBusy = false;
    if (result1.ptr()) {
        if (result2.ptr() && (result1 != result2))
            error(name, "ambiguous imported symbol: " + name->str);
        return result1;
    }
    else {
        return result2;
    }
}



//
// addLocal, lookupEnv
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
