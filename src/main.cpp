#include "clay.hpp"
#include <iostream>
#include <cstring>

using namespace std;

static void setSearchPath(const char *exePath) {
    const char *end = exePath + strlen(exePath);
    while (end != exePath) {
        if ((end[-1] == '\\') || (end[-1] == '/'))
            break;
        --end;
    }
    string dirPath(exePath, end);
    addSearchPath(dirPath + "lib-clay");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        cerr << "usage: " << argv[0] << " <clayfile>\n";
        return -1;
    }
    initLLVM();
    initTypes();
    initVoidValue();

    setSearchPath(argv[0]);

    ModulePtr m = loadProgram(argv[1]);
    ObjectPtr mainProc = lookupEnv(m->env, new Identifier("main"));

    cout << "analyzing main()\n";
    ReturnInfoPtr rinfo = analyzeInvoke(mainProc, vector<AnalysisPtr>());
    cout << "returns " << rinfo->type << " by ";
    cout << (rinfo->isRef ? "ref" : "value") << '\n';

    cout << '\n';

    cout << "evaluating main()\n";
    ValuePtr result = invoke(mainProc, vector<ValuePtr>());
    cout << result->type << '\n';
    cout << result << '\n';

    return 0;
}
