#include "clay.hpp"
#include <cstdio>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <clayfile>\n", argv[0]);
        return -1;
    }
    SourcePtr source = loadFile(argv[1]);
    ModulePtr m = parse(source);
    printf("parsed successfully\n");
    return 0;
}
