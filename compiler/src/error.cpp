#include "clay.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>



//
// invoke stack - a compilation call stack
//

static vector<CompileContextEntry> contextStack;

void pushCompileContext(ObjectPtr obj, const vector<ObjectPtr> &params) {
    if (contextStack.size() > 1000)
        error("runaway recursion");
    contextStack.push_back(make_pair(obj, params));
}

void popCompileContext() {
    contextStack.pop_back();
}

vector<CompileContextEntry> getCompileContext() {
    return contextStack;
}

void setCompileContext(const vector<CompileContextEntry> &x) {
    contextStack = x;
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

LocationPtr topLocation() {
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

static void computeLineCol(LocationPtr location,
                           int &line,
                           int &column,
                           int &tabColumn) {
    char *p = location->source->data;
    char *end = p + location->offset;
    line = column = tabColumn = 0;
    for (; p != end; ++p) {
        ++column;
        ++tabColumn;
        if (*p == '\n') {
            ++line;
            column = 0;
            tabColumn = 0;
        }
        else if (*p == '\t') {
            tabColumn += 7;
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
    int tabColumn;
    computeLineCol(location, line, column, tabColumn);
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
            for (int j = 0; j < tabColumn; ++j)
                fprintf(stderr, "-");
            fprintf(stderr, "^\n");
        }
    }
    fprintf(stderr, "###############################\n");
}

static void displayCompileContext() {
    if (contextStack.empty())
        return;
    fprintf(stderr, "\n");
    fprintf(stderr, "compilation context: \n");
    for (unsigned i = contextStack.size(); i > 0; --i) {
        ObjectPtr obj = contextStack[i-1].first;
        const vector<ObjectPtr> &params = contextStack[i-1].second;

        ostringstream sout;
        if (obj->objKind == GLOBAL_VARIABLE) {
            sout << "global ";
            printName(sout, obj);
            if (!params.empty()) {
                sout << "[";
                printNameList(sout, params);
                sout << "]";
            }
        }
        else {
            printName(sout, obj);
            sout << "(";
            printNameList(sout, params);
            sout << ")";
        }
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

static bool abortOnError = false;

void setAbortOnError(bool flag) {
    abortOnError = flag;
}

void error(const string &msg) {
    LocationPtr location = topLocation();
    if (location.ptr()) {
        int line, column;
        displayLocation(location, line, column);
        fprintf(stderr, "%s(%d,%d): error: %s\n",
                location->source->fileName.c_str(),
                line+1, column, msg.c_str());
        displayCompileContext();
        displayDebugStack();
    }
    else {
        fprintf(stderr, "error: %s\n", msg.c_str());
    }
    if (abortOnError)
        abort();
    else
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

void argumentError(unsigned int index, const string &msg) {
    ostringstream sout;
    sout << "argument " << (index+1) << ": " << msg;
    error(sout.str());
}

static const char *valuesStr(int n) {
    if (n == 1)
        return "value";
    else
        return "values";
}

void arityError(int expected, int received) {
    ostringstream sout;
    sout << "expected " << expected << " " << valuesStr(expected);
    sout << ", but received " << received << " " << valuesStr(received);
    error(sout.str());
}

void arityError2(int minExpected, int received) {
    ostringstream sout;
    sout << "expected atleast " << minExpected
         << " " << valuesStr(minExpected);
    sout << ", but received " << received << " " << valuesStr(received);
    error(sout.str());
}

void ensureArity(MultiStaticPtr args, unsigned int size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void ensureArity(MultiEValuePtr args, unsigned int size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void ensureArity(MultiPValuePtr args, unsigned int size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void ensureArity(MultiCValuePtr args, unsigned int size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void arityMismatchError(int leftArity, int rightArity) {
    ostringstream sout;
    sout << "arity mismatch. ";
    sout << "left-side has " << leftArity << " " << valuesStr(leftArity);
    sout << ", whereas right-side has " << rightArity
         << " " << valuesStr(rightArity);
}

static string typeErrorMessage(const string &expected,
                               TypePtr receivedType) {
    ostringstream sout;
    sout << "expected " << expected << ", "
         << "but received " << receivedType << " type";
    return sout.str();
}

static string typeErrorMessage(TypePtr expectedType,
                               TypePtr receivedType) {
    ostringstream sout;
    sout << expectedType << " type";
    return typeErrorMessage(sout.str(), receivedType);
}

void typeError(const string &expected, TypePtr receivedType) {
    error(typeErrorMessage(expected, receivedType));
}

void typeError(TypePtr expectedType, TypePtr receivedType) {
    error(typeErrorMessage(expectedType, receivedType));
}

void argumentTypeError(unsigned int index,
                       const string &expected,
                       TypePtr receivedType) {
    argumentError(index, typeErrorMessage(expected, receivedType));
}

void argumentTypeError(unsigned int index,
                       TypePtr expectedType,
                       TypePtr receivedType) {
    argumentError(index, typeErrorMessage(expectedType, receivedType));
}

void indexRangeError(const string &kind,
                     size_t value,
                     size_t maxValue)
{
    ostringstream sout;
    sout << kind << " " << value << " is out of range. ";
    sout << "it should be less than " << maxValue;
    error(sout.str());
}

void argumentIndexRangeError(unsigned int index,
                             const string &kind,
                             size_t value,
                             size_t maxValue)
{
    ostringstream sout;
    sout << kind << " " << value << " is out of range. ";
    sout << "it should be less than " << maxValue;
    argumentError(index, sout.str());
}

void invalidStaticObjectError(ObjectPtr obj)
{
    ostringstream sout;
    sout << "invalid static object: " << obj;
    error(sout.str());
}

void argumentInvalidStaticObjectError(unsigned int index, ObjectPtr obj)
{
    ostringstream sout;
    sout << "invalid static object: " << obj;
    argumentError(index, sout.str());
}
