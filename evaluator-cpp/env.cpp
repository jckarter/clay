#include "clay.hpp"
#include <cassert>

using namespace std;

typedef map<string, ObjectPtr>::iterator MapIter;



//
// addGlobal, lookupGlobal
//

void addGlobal(ModulePtr module, IdentifierPtr name, ObjectPtr value) {
    MapIter i = module->globals.find(name->str);
    if (i != module->globals.end())
        error(name, "name redefined: " + name->str);
    module->globals[name->str] = value;
}

ObjectPtr lookupGlobal(ModulePtr module, IdentifierPtr name) {
    MapIter i = module->globals.find(name->str);
    if (i != module->globals.end())
        return i->second;
    vector<ImportPtr> &x = module->imports;
    vector<ImportPtr>::iterator ii, iend;
    ObjectPtr result;
    for (ii = x.begin(), iend = x.end(); ii != iend; ++ii) {
        ObjectPtr entry = lookupPublic((*ii)->module, name);
        if (entry) {
            if (!result)
                result = entry;
            else if (result != entry)
                error(name, "ambiguous imported symbol: " + name->str);
        }
    }
    if (!result)
        error(name, "undefined name: " + name->str);
    return result;
}



//
// lookupPublic
//

ObjectPtr lookupPublic(ModulePtr module, IdentifierPtr name) {
    if (module->lookupBusy)
        return NULL;
    MapIter i = module->globals.find(name->str);
    if (i != module->globals.end())
        return i->second;
    module->lookupBusy = true;
    vector<ExportPtr> &x = module->exports;
    vector<ExportPtr>::iterator ei, eend;
    ObjectPtr result;
    for (ei = x.begin(), eend = x.end(); ei != eend; ++ei) {
        ObjectPtr entry = lookupPublic((*ei)->module, name);
        if (entry) {
            if (!result)
                result = entry;
            else if (result != entry)
                error(name, "ambiguous public symbol: " + name->str);
        }
    }
    module->lookupBusy = false;
    return result;
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
    if (env->parent) {
        switch (env->parent->objKind) {
        case ENV : {
            Env *y = (Env *)env->parent.raw();
            return lookupEnv(y, name);
        }
        case MODULE : {
            Module *y = (Module *)env->parent.raw();
            return lookupGlobal(y, name);
        }
        default :
            assert(false);
        }
    }
    error(name, "undefined name: " + name->str);
    return NULL;
}
