#include "clay.hpp"
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
    if (argc != 2) {
        cerr << "usage: " << argv[0] << " <clayfile>\n";
        return -1;
    }
    SourcePtr source = loadFile(argv[1]);
    ModulePtr m = parse(source);
    
    cout << m << '\n';
    return 0;
}
