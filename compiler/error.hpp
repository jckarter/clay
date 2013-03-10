#ifndef __ERROR_HPP
#define __ERROR_HPP

#include "clay.hpp"
#include "invoketables.hpp"

namespace clay {

void error(llvm::Twine const &msg);
void error(Location const& location, llvm::Twine const &msg);

void warning(llvm::Twine const &msg);

void fmtError(const char *fmt, ...);

template <class T>
inline void error(Pointer<T> context, llvm::Twine const &msg)
{
    if (context->location.ok())
        pushLocation(context->location);
    error(msg);
}

template <class T>
inline void error(T const *context, llvm::Twine const &msg)
{
    error(context->location, msg);
}

void argumentError(size_t index, llvm::StringRef msg);

void arityError(size_t expected, size_t received);
void arityError2(size_t minExpected, size_t received);

template <class T>
inline void arityError(Pointer<T> context, size_t expected, size_t received)
{
    if (context->location.ok())
        pushLocation(context->location);
    arityError(expected, received);
}

template <class T>
inline void arityError2(Pointer<T> context, size_t minExpected, size_t received)
{
    if (context->location.ok())
        pushLocation(context->location);
    arityError2(minExpected, received);
}

void ensureArity(MultiStaticPtr args, size_t size);
void ensureArity(MultiEValuePtr args, size_t size);
void ensureArity(MultiPValuePtr args, size_t size);
void ensureArity(MultiCValuePtr args, size_t size);

template <class T>
inline void ensureArity(T const &args, size_t size)
{
    if (args.size() != size)
        arityError(size, args.size());
}

template <class T>
inline void ensureArity2(T const &args, size_t size, bool hasVarArgs)
{
    if (!hasVarArgs)
        ensureArity(args, size);
    else if (args.size() < size)
        arityError2(size, args.size());
}

void arityMismatchError(size_t leftArity, size_t rightArity, bool hasVarArg);

void typeError(llvm::StringRef expected, TypePtr receivedType);
void typeError(TypePtr expectedType, TypePtr receivedType);

void argumentTypeError(unsigned index,
                       llvm::StringRef expected,
                       TypePtr receivedType);
void argumentTypeError(unsigned index,
                       TypePtr expectedType,
                       TypePtr receivedType);

void indexRangeError(llvm::StringRef kind,
                     size_t value,
                     size_t maxValue);

void argumentIndexRangeError(unsigned index,
                             llvm::StringRef kind,
                             size_t value,
                             size_t maxValue);

extern bool shouldPrintFullMatchErrors;
extern set<pair<string,string> > logMatchSymbols;

void matchBindingError(MatchResultPtr const &result);
void matchFailureLog(MatchFailureError const &err);
void matchFailureError(MatchFailureError const &err);

class CompilerError : std::exception {
};

}

#endif // __ERROR_HPP
