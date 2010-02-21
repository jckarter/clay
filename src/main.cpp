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

static void usage()
{
    cerr << "usage: clayc <clayfile>\n";
}

int main(int argc, char **argv) {
    if (argc != 2) {
        usage();
        return -1;
    }

    const char *clayFile = argv[argc-1];

    initLLVM();
    initTypes();

    setSearchPath(argv[0]);

    ModulePtr m = loadProgram(clayFile);
    codegenMain(m);

    llvm::raw_stdout_ostream out;
    out << *llvmModule;

    return 0;
}
