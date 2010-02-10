#include "clay.hpp"
#include <iostream>
#include <cstring>

using namespace std;

static void setSearchPath(const char *exePath)
{
    const char *end = exePath + strlen(exePath);
    while (end != exePath) {
        if ((end[-1] == '\\') || (end[-1] == '/'))
            break;
        --end;
    }
    string dirPath(exePath, end);
    addSearchPath(dirPath + "lib-clay");
}

static void evalProgram(ModulePtr m)
{
    ObjectPtr mainProc = lookupEnv(m->env, new Identifier("main"));

    cout << "analyzing main()\n";
    ArgListPtr args = new ArgList(vector<ExprPtr>(), new Env());
    PValuePtr pvalue = partialInvoke(mainProc, args);
    cout << "returns " << pvalue->type << " by ";
    cout << (pvalue->isTemp ? "value" : "ref") << '\n';

    cout << '\n';

    cout << "evaluating main()\n";
    ValuePtr result = invoke(mainProc, vector<ValuePtr>());
    cout << result->type << '\n';
    cout << result << '\n';
}

static int runProgram(ModulePtr m)
{
    llvm::Function *llMain = codegenMain(m);
    vector<llvm::GenericValue> llArgs;
    llvm::GenericValue result = llvmEngine->runFunction(llMain, llArgs);
    return (int)result.IntVal.getSExtValue();
}

static void compileProgram(ModulePtr m)
{
    codegenMain(m);
    llvm::raw_stdout_ostream out;
    out << *llvmModule;
}

static void usage()
{
    cerr << "usage: clayc [--eval | --run] <clayfile>\n";
}

int main(int argc, char **argv) {
    if ((argc != 2) && (argc != 3)) {
        usage();
        return -1;
    }

    bool doEval = false;
    bool doRun = false;
    if (argc == 3) {
        if (strcmp(argv[1], "--eval") == 0) {
            doEval = true;
        }
        else if (strcmp(argv[1], "--run") == 0) {
            doRun = true;
        }
        else {
            usage();
            return -1;
        }
    }
    const char *clayFile = argv[argc-1];

    initLLVM();
    initTypes();
    initVoidValue();

    setSearchPath(argv[0]);

    ModulePtr m = loadProgram(clayFile);
    if (doEval) {
        evalProgram(m);
        return 0;
    }
    else if (doRun) {
        return runProgram(m);
    }
    else {
        compileProgram(m);
        return 0;
    }
}
