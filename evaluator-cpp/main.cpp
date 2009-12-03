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

    setSearchPath(argv[0]);

    ModulePtr m = loadProgram(argv[1]);
    
    cout << m << '\n';
    return 0;
}
