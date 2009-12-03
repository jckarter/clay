#include "clay.hpp"
#include <cstdio>

using namespace std;

#ifdef WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

static vector<string> searchPath;
static map<string, ModulePtr> modules;



//
// addSearchPath, locateFile
//

void addSearchPath(const string &path) {
    searchPath.push_back(path);
}

static bool locateFile(const string &relativePath, string &path) {
    string tail = PATH_SEPARATOR + relativePath;
    vector<string>::iterator i, end;
    for (i = searchPath.begin(), end = searchPath.end(); i != end; ++i) {
        string p = (*i) + tail;
        FILE *f = fopen(p.c_str(), "rb");
        if (f != NULL) {
            fclose(f);
            path = p;
            return true;
        }
    }
    return false;
}



//
// toRelativePath, toKey
//

static string toRelativePath(DottedNamePtr name) {
    string relativePath;
    vector<IdentifierPtr>::iterator i, end;
    bool first = true;
    for (i = name->parts.begin(), end = name->parts.end(); i != end; ++i) {
        if (!first)
            relativePath.push_back(PATH_SEPARATOR);
        else
            first = false;
        relativePath.append((*i)->str);
    }
    relativePath.append(".clay");
    return relativePath;
}

static string toKey(DottedNamePtr name) {
    string key;
    vector<IdentifierPtr>::iterator i, end;
    bool first = true;
    for (i = name->parts.begin(), end = name->parts.end(); i != end; ++i) {
        if (!first)
            key.push_back('.');
        else
            first = false;
        key.append((*i)->str);
    }
    return key;
}



//
// loadModuleByName, loadDependents, loadProgram
//

static void loadDependents(ModulePtr m);
static ModulePtr makePrimitivesModule();

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

    return module;
}

static void loadDependents(ModulePtr m) {
    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii)
        (*ii)->module = loadModuleByName((*ii)->dottedName);
    vector<ExportPtr>::iterator ei, eend;
    for (ei = m->exports.begin(), eend = m->exports.end(); ei != eend; ++ei)
        (*ei)->module = loadModuleByName((*ei)->dottedName);
}

ModulePtr loadProgram(const string &fileName) {
    ModulePtr m = parse(loadFile(fileName));
    loadDependents(m);
    return m;
}



//
// makePrimitivesModule
//

static ModulePtr makePrimitivesModule() {
    return new Module();
}
