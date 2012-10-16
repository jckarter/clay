#include "clay.hpp"

namespace clay {

bool shouldPrintFullMatchErrors;
set<pair<string,string> > logMatchSymbols;


//
// invoke stack - a compilation call stack
//

static vector<CompileContextEntry> contextStack;

static const unsigned RECURSION_WARNING_LEVEL = 1000;

void pushCompileContext(ObjectPtr obj) {
    if (contextStack.size() >= RECURSION_WARNING_LEVEL)
        warning("potential runaway recursion");
    if (!contextStack.empty())
        contextStack.back().location = topLocation();
    contextStack.push_back(CompileContextEntry(obj));
}

void pushCompileContext(ObjectPtr obj, const vector<ObjectPtr> &params) {
    if (contextStack.size() >= RECURSION_WARNING_LEVEL)
        warning("potential runaway recursion");
    if (!contextStack.empty())
        contextStack.back().location = topLocation();
    contextStack.push_back(CompileContextEntry(obj, params));
}

void pushCompileContext(ObjectPtr obj, const vector<ObjectPtr> &params, const vector<unsigned> &dispatchIndices) {
    if (contextStack.size() >= RECURSION_WARNING_LEVEL)
        warning("potential runaway recursion");
    if (!contextStack.empty())
        contextStack.back().location = topLocation();
    contextStack.push_back(CompileContextEntry(obj, params, dispatchIndices));
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

static vector<Location> errorLocations;

void pushLocation(Location const& location) {
    errorLocations.push_back(location);
}

void popLocation() {
    errorLocations.pop_back();
}

Location topLocation() {
    vector<Location>::iterator i, begin;
    i = errorLocations.end();
    begin = errorLocations.begin();
    while (i != begin) {
        --i;
        if (i->ok()) return *i;
    }
    return Location();
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
        llvm::outs() << ' ';
    llvm::outs() << "BEGIN - " << obj << '\n';
    ++indent;
    debugStack.push_back(obj);
}

DebugPrinter::~DebugPrinter()
{
    debugStack.pop_back();
    --indent;
    for (int i = 0; i < indent; ++i)
        llvm::outs() << ' ';
    llvm::outs() << "DONE - " << obj << '\n';
}



//
// report error
//

static void computeLineCol(Location const &location, int &line, int &column, int &tabColumn) {
    const char *p = location.source->data();
    const char *end = p + location.offset;
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

llvm::DIFile getDebugLineCol(Location const &location, int &line, int &column) {
    if (!location.ok()) {
        line = 0;
        column = 0;
        return llvm::DIFile(NULL);
    }

    int tabColumn;
    computeLineCol(location, line, column, tabColumn);
    line += 1;
    column += 1;
    return location.source->getDebugInfo();
}

void getLineCol(Location const &location, int &line, int &column, int &tabColumn) {
    if (!location.ok()) {
        line = 0;
        column = 0;
        tabColumn = 0;
        return;
    }

    computeLineCol(location, line, column, tabColumn);
}

static void splitLines(SourcePtr source, vector<string> &lines) {
    lines.push_back(string());
    const char *p = source->data();
    const char *end = source->endData();
    for (; p != end; ++p) {
        lines.back().push_back(*p);
        if (*p == '\n')
            lines.push_back(string());
    }
}

static bool endsWithNewline(llvm::StringRef s) {
    if (s.size() == 0) return false;
    return s[s.size()-1] == '\n';
}

static void displayLocation(Location const &location, int &line, int &column) {
    int tabColumn;
    getLineCol(location, line, column, tabColumn);
    vector<string> lines;
    splitLines(location.source, lines);
    llvm::errs() << "###############################\n";
    for (int i = line-2; i <= line+2; ++i) {
        if ((i < 0) || (i >= (int)lines.size()))
            continue;
        llvm::errs() << lines[i];
        if (!endsWithNewline(lines[i]))
            llvm::errs() << "\n";
        if (i == line) {
            for (int j = 0; j < tabColumn; ++j)
                llvm::errs() << "-";
            llvm::errs() << "^\n";
        }
    }
    llvm::errs() << "###############################\n";
}

// This has to use stdio because it needs to be usable from the debugger
// and cerr or errs may be destroyed if there's a bug in global dtors
extern "C" void displayCompileContext() {
    if (contextStack.empty())
        return;
    fprintf(stderr, "\ncompilation context: \n");
    string buf;
    llvm::raw_string_ostream errs(buf);
    for (size_t i = contextStack.size(); i > 0; --i) {
        ObjectPtr obj = contextStack[i-1].callable;
        const vector<ObjectPtr> &params = contextStack[i-1].params;

        if (i < contextStack.size() && contextStack[i-1].location.ok()) {
            errs << "  ";
            printFileLineCol(errs, contextStack[i-1].location);
            errs << ":\n";
        }

        errs << "    ";
        if (obj->objKind == GLOBAL_VARIABLE) {
            errs << "global ";
            printName(errs, obj);
            if (!params.empty()) {
                errs << "[";
                printNameList(errs, params);
                errs << "]";
            }
        }
        else {
            printName(errs, obj);
            if (contextStack[i-1].hasParams) {
                errs << "(";
                printNameList(errs, params, contextStack[i-1].dispatchIndices);
                errs << ")";
            }
        }
        errs << "\n";
    }
    fprintf(stderr, "%s", errs.str().c_str());
    fflush(stderr);
}

static void displayDebugStack() {
    if (debugStack.empty())
        return;
    llvm::errs() << "\ndebug stack:\n";
    for (size_t i = debugStack.size(); i > 0; --i) {
        llvm::errs() << "  " << debugStack[i-1] << "\n";
    }
}

void displayError(llvm::Twine const &msg, llvm::StringRef kind) {
    string msgString = msg.str();
    if (msgString.empty() || msgString[msgString.length() - 1] != '\n')
        msgString += '\n';

    Location location = topLocation();
    if (location.ok()) {
        int line, column;
        displayLocation(location, line, column);
        llvm::errs() << location.source->fileName
            << '(' << line+1 << ',' << column << "): " << kind << ": " << msgString;
        llvm::errs().flush();
        displayCompileContext();
        displayDebugStack();
    }
    else {
        llvm::errs() << kind << ": " << msgString;
    }
}

void warning(llvm::Twine const &msg) {
    displayError(msg, "warning");
}

void note(llvm::Twine const &msg) {
    displayError(msg, "note");
}

void error(llvm::Twine const &msg) {
    displayError(msg, "error");
    throw CompilerError();
}

void error(Location const &location, llvm::Twine const &msg) {
    if (location.ok())
        pushLocation(location);
    error(msg);
}

void fmtError(const char *fmt, ...) {
    va_list ap;
    char s[256];
    va_start(ap, fmt);
    vsnprintf(s, sizeof(s)-1, fmt, ap);
    va_end(ap);
    error(s);
}

void argumentError(unsigned int index, llvm::StringRef msg) {
    string buf;
    llvm::raw_string_ostream sout(buf);
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
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "expected " << expected << " " << valuesStr(expected);
    sout << ", but received " << received << " " << valuesStr(received);
    error(sout.str());
}

void arityError2(int minExpected, int received) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "expected at least " << minExpected
         << " " << valuesStr(minExpected);
    sout << ", but received " << received << " " << valuesStr(received);
    error(sout.str());
}

void ensureArity(MultiStaticPtr args, size_t size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void ensureArity(MultiEValuePtr args, size_t size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void ensureArity(MultiPValuePtr args, size_t size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void ensureArity(MultiCValuePtr args, size_t size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void arityMismatchError(int leftArity, int rightArity, bool hasVarArg) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    if (hasVarArg)
        sout << "left side takes " << leftArity << " or more " << valuesStr(leftArity);
    else
        sout << "left side has " << leftArity << " " << valuesStr(leftArity);
    sout << ", but right side has " << rightArity
         << " " << valuesStr(rightArity);
    error(sout.str());
}

static string typeErrorMessage(llvm::StringRef expected,
                               TypePtr receivedType) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "expected " << expected << ", "
         << "but received " << receivedType << " type";
    return sout.str();
}

static string typeErrorMessage(TypePtr expectedType,
                               TypePtr receivedType) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << expectedType << " type";
    return typeErrorMessage(sout.str(), receivedType);
}

void typeError(llvm::StringRef expected, TypePtr receivedType) {
    error(typeErrorMessage(expected, receivedType));
}

void typeError(TypePtr expectedType, TypePtr receivedType) {
    error(typeErrorMessage(expectedType, receivedType));
}

void argumentTypeError(unsigned int index,
                       llvm::StringRef expected,
                       TypePtr receivedType) {
    argumentError(index, typeErrorMessage(expected, receivedType));
}

void argumentTypeError(unsigned int index,
                       TypePtr expectedType,
                       TypePtr receivedType) {
    argumentError(index, typeErrorMessage(expectedType, receivedType));
}

void indexRangeError(llvm::StringRef kind,
                     size_t value,
                     size_t maxValue)
{
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << kind << " " << value << " is out of range. ";
    sout << "it should be less than " << maxValue;
    error(sout.str());
}

void argumentIndexRangeError(unsigned int index,
                             llvm::StringRef kind,
                             size_t value,
                             size_t maxValue)
{
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << kind << " " << value << " is out of range. ";
    sout << "it should be less than " << maxValue;
    argumentError(index, sout.str());
}

void invalidStaticObjectError(ObjectPtr obj)
{
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "invalid static object: " << obj;
    error(sout.str());
}

void argumentInvalidStaticObjectError(unsigned int index, ObjectPtr obj)
{
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "invalid static object: " << obj;
    argumentError(index, sout.str());
}

void matchBindingError(MatchResultPtr result)
{
    string buf;
    llvm::raw_string_ostream sout(buf);
    printMatchError(sout, result);
    error(sout.str());
}

static void matchFailureMessage(MatchFailureError const &err, string &outBuf)
{
    llvm::raw_string_ostream sout(outBuf);
    int hiddenPatternOverloads = 0;

    for (MatchFailureVector::const_iterator i = err.failures.begin();
         i != err.failures.end();
         ++i)
    {
        OverloadPtr overload = i->first;
        if (!shouldPrintFullMatchErrors && overload->nameIsPattern) {
            ++hiddenPatternOverloads;
            continue;
        }
        sout << "\n    ";
        Location location = overload->location;
        int line, column, tabColumn;
        getLineCol(location, line, column, tabColumn);
        sout << location.source->fileName.c_str()
            << "(" << line+1 << "," << column << ")"
            << "\n        ";
        printMatchError(sout, i->second);
    }
    if (hiddenPatternOverloads > 0)
        sout << "\n    " << hiddenPatternOverloads << " universal overloads not shown (show with -full-match-errors option)";
    sout.flush();
}

void matchFailureError(MatchFailureError const &err)
{
    string buf;
    if (err.failedInterface)
        buf = "call does not conform to function interface";
    else
        buf = "no matching overload found";

    matchFailureMessage(err, buf);
    error(buf);
}

void matchFailureLog(MatchFailureError const &err)
{
    if (err.failures.empty())
        return;
    string buf = "matched";
    matchFailureMessage(err, buf);
    note(buf);
}

void printFileLineCol(llvm::raw_ostream &out, Location const &location)
{
    int line, column, tabColumn;
    getLineCol(location, line, column, tabColumn);
    out << location.source->fileName << "(" << line+1 << "," << column << ")";
}

}
