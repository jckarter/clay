#include "clay.hpp"
#include <cstdio>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <clayfile>\n", argv[0]);
        return -1;
    }
    SourcePtr source = loadFile(argv[1]);
    vector<TokenPtr> tokens;
    tokenize(source, tokens);
    vector<TokenPtr>::iterator i, end;
    for (i = tokens.begin(), end = tokens.end(); i != end; ++i) {
        TokenPtr t = *i;
        const char *name = tokenName(t->tokenKind);
        printf("%s '%s'\n", name, t->str.c_str());
    }
    return 0;
}
