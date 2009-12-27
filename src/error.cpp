#include "clay.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

static vector<LocationPtr> *errorLocations = NULL;

static void init() {
    if (errorLocations == NULL)
        errorLocations = new vector<LocationPtr>();
}

void pushLocation(LocationPtr location) {
    init();
    errorLocations->push_back(location);
}

void popLocation() {
    errorLocations->pop_back();
}

static LocationPtr topLocation() {
    init();
    vector<LocationPtr>::iterator i, begin;
    i = errorLocations->end();
    begin = errorLocations->begin();
    while (i != begin) {
        --i;
        if (i->raw()) return *i;
    }
    return NULL;
}

static void computeLineCol(LocationPtr location, int &line, int &column) {
    char *p = location->source->data;
    char *end = p + location->offset;
    line = column = 0;
    for (; p != end; ++p) {
        ++column;
        if (*p == '\n') {
            ++line;
            column = 0;
        }
    }
}

static void splitLines(SourcePtr source, vector<string> &lines) {
    lines.push_back(string());
    char *p = source->data;
    char *end = p + source->size;
    for (; p != end; ++p) {
        lines.back().push_back(*p);
        if (*p == '\n')
            lines.push_back(string());
    }
}

static bool endsWithNewline(const string& s) {
    if (s.size() == 0) return false;
    return s[s.size()-1] == '\n';
}

static void displayLocation(LocationPtr location, int &line, int &column) {
    computeLineCol(location, line, column);
    vector<string> lines;
    splitLines(location->source, lines);
    fprintf(stderr, "###############################\n");
    for (int i = line-2; i <= line+2; ++i) {
        if ((i < 0) || (i >= (int)lines.size()))
            continue;
        fprintf(stderr, "%s", lines[i].c_str());
        if (!endsWithNewline(lines[i]))
            fprintf(stderr, "\n");
        if (i == line) {
            for (int j = 0; j < column; ++j)
                fprintf(stderr, "-");
            fprintf(stderr, "^\n");
        }
    }
    fprintf(stderr, "###############################\n");
}

void error(const string &msg) {
    LocationPtr location = topLocation();
    if (location.raw()) {
        int line, column;
        displayLocation(location, line, column);
        fprintf(stderr, "%s(%d,%d): error: %s\n",
                location->source->fileName.c_str(),
                line+1, column, msg.c_str());
    }
    else {
        fprintf(stderr, "error: %s\n", msg.c_str());
    }
    exit(-1);
}

void fmtError(const char *fmt, ...) {
    va_list ap;
    char s[256];
    va_start(ap, fmt);
    vsnprintf(s, sizeof(s)-1, fmt, ap);
    va_end(ap);
    error(s);
}



//
// error checking helper procs
//

void ensurePrimitiveType(TypePtr t) {
    if (t->typeKind == RECORD_TYPE)
        error("primitive type expected");
}

void ensureSameType(TypePtr ta, TypePtr tb) {
    if (ta != tb)
        error("type mismatch");
}

void ensureNumericType(TypePtr t) {
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return;
    }
    error("numeric type expected");
}

void ensureIntegerType(TypePtr t) {
    if (t->typeKind != INTEGER_TYPE)
        error("integer type expected");
}

void ensurePointerType(TypePtr t) {
    if (t->typeKind != POINTER_TYPE)
        error("pointer type expected");
}

void ensureArrayType(TypePtr t) {
    if (t->typeKind != ARRAY_TYPE)
        error("array type expected");
}

void ensureTupleType(TypePtr t) {
    if (t->typeKind != TUPLE_TYPE)
        error("tuple type expected");
}

void ensureRecordType(TypePtr t) {
    if (t->typeKind != RECORD_TYPE)
        error("record type expected");
}
