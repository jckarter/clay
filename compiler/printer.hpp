#ifndef __PRINTER_HPP
#define __PRINTER_HPP

#include "clay.hpp"

namespace clay {


inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const clay_int128 &x) {
    return os << ptrdiff64_t(x);
}
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const clay_uint128 &x) {
    return os << size64_t(x);
}


template <class T>
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &out, llvm::ArrayRef<T> v)
{
    out << "[";
    const T *i, *end;
    bool first = true;
    for (i = v.begin(), end = v.end(); i != end; ++i) {
        if (!first)
            out << ", ";
        first = false;
        out << *i;
    }
    out << "]";
    return out;
}


//
// printer module
//

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Object &obj);

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Object *obj);

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, PVData const &pv);

template <class T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Pointer<T> &p)
{
    out << *p;
    return out;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const PatternVar &pvar);

void enableSafePrintName();
void disableSafePrintName();

struct SafePrintNameEnabler {
    SafePrintNameEnabler() { enableSafePrintName(); }
    ~SafePrintNameEnabler() { disableSafePrintName(); }
};

void printNameList(llvm::raw_ostream &out, llvm::ArrayRef<ObjectPtr> x);
void printNameList(llvm::raw_ostream &out, llvm::ArrayRef<ObjectPtr> x, llvm::ArrayRef<unsigned> dispatchIndices);
void printNameList(llvm::raw_ostream &out, llvm::ArrayRef<TypePtr> x);
void printStaticName(llvm::raw_ostream &out, ObjectPtr x);
void printName(llvm::raw_ostream &out, ObjectPtr x);
void printTypeAndValue(llvm::raw_ostream &out, EValuePtr ev);
void printValue(llvm::raw_ostream &out, EValuePtr ev);

string shortString(llvm::StringRef in);





}

#endif // __PRINTER_HPP
