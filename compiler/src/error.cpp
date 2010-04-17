#include "clay.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>



//
// invoke stack - a compilation call stack
//

typedef pair<ObjectPtr, const vector<ObjectPtr> *> InvokeStackEntry;

static vector<InvokeStackEntry> invokeStack;

void pushInvokeStack(ObjectPtr callable, const vector<ObjectPtr> &argsKey) {
    invokeStack.push_back(make_pair(callable, &argsKey));
}

void popInvokeStack() {
    invokeStack.pop_back();
}



//
// source location of the current item being processed
//

static vector<LocationPtr> errorLocations;

void pushLocation(LocationPtr location) {
    errorLocations.push_back(location);
}

void popLocation() {
    errorLocations.pop_back();
}

static LocationPtr topLocation() {
    vector<LocationPtr>::iterator i, begin;
    i = errorLocations.end();
    begin = errorLocations.begin();
    while (i != begin) {
        --i;
        if (i->ptr()) return *i;
    }
    return NULL;
}



//
// DebugPrinter
//

static vector<ObjectPtr> debugStack;

int DebugPrinter::indent = 0;

DebugPrinter::DebugPrinter(ObjectPtr obj)
    : obj(obj)
{
    for (int i = 0; i < indent; ++i)
        std::cout << ' ';
    std::cout << "BEGIN - " << obj << '\n';
    ++indent;
    debugStack.push_back(obj);
}

DebugPrinter::~DebugPrinter()
{
    debugStack.pop_back();
    --indent;
    for (int i = 0; i < indent; ++i)
        std::cout << ' ';
    std::cout << "DONE - " << obj << '\n';
}



//
// report error
//

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

static void displayInvokeStack() {
    if (invokeStack.empty())
        return;
    fprintf(stderr, "\n");
    fprintf(stderr, "compilation context: \n");
    for (unsigned i = invokeStack.size(); i > 0; --i) {
        ObjectPtr callable = invokeStack[i-1].first;
        const vector<ObjectPtr> &argsKey = *invokeStack[i-1].second;

        ostringstream sout;
        printName(sout, callable);
        sout << "(";
        printNameList(sout, argsKey);
        sout << ")";
        fprintf(stderr, "  %s\n", sout.str().c_str());
    }
}

static void displayDebugStack() {
    if (debugStack.empty())
        return;
    fprintf(stderr, "\n");
    fprintf(stderr, "debug stack: \n");
    for (unsigned i = debugStack.size(); i > 0; --i) {
        ostringstream sout;
        sout << debugStack[i-1];
        fprintf(stderr, "  %s\n", sout.str().c_str());
    }
}

void error(const string &msg) {
    LocationPtr location = topLocation();
    if (location.ptr()) {
        int line, column;
        displayLocation(location, line, column);
        fprintf(stderr, "%s(%d,%d): error: %s\n",
                location->source->fileName.c_str(),
                line+1, column, msg.c_str());
        displayInvokeStack();
        displayDebugStack();
    }
    else {
        fprintf(stderr, "error: %s\n", msg.c_str());
    }
    //abort();
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
