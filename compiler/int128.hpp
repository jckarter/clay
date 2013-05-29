#pragma once

#include "clay.hpp"

//
// int128 type
//


#if defined(__GNUC__) && defined(_INT128_DEFINED)

namespace clay {

typedef __int128 clay_int128;
typedef unsigned __int128 clay_uint128;

}

//#elif (defined(__clang__))
//typedef __int128_t clay_int128;
//typedef __uint128_t clay_uint128;

#else

namespace clay {

// fake it by doing 64-bit math in a 128-bit padded container
struct uint128_holder;

struct int128_holder {
    ptrdiff64_t lowValue;
    ptrdiff64_t highPad; // not used in static math

    int128_holder() {}
    explicit int128_holder(ptrdiff64_t low) : lowValue(low), highPad(low < 0 ? -1 : 0) {}
    int128_holder(ptrdiff64_t low, ptrdiff64_t high)
        : lowValue(low), highPad(high) {}
    explicit int128_holder(uint128_holder y);

    int128_holder &operator=(ptrdiff64_t low) {
        new ((void*)this) int128_holder(low);
        return *this;
    }

    int128_holder operator-() const { return int128_holder(-lowValue); }
    int128_holder operator~() const { return int128_holder(~lowValue); }

    bool operator==(int128_holder y) const { return lowValue == y.lowValue; }
    bool operator< (int128_holder y) const { return lowValue <  y.lowValue; }

    int128_holder operator+ (int128_holder y) const { return int128_holder(lowValue +  y.lowValue); }
    int128_holder operator- (int128_holder y) const { return int128_holder(lowValue -  y.lowValue); }
    int128_holder operator* (int128_holder y) const { return int128_holder(lowValue *  y.lowValue); }
    int128_holder operator/ (int128_holder y) const { return int128_holder(lowValue /  y.lowValue); }
    int128_holder operator% (int128_holder y) const { return int128_holder(lowValue %  y.lowValue); }
    int128_holder operator<<(int128_holder y) const { return int128_holder(lowValue << y.lowValue); }
    int128_holder operator>>(int128_holder y) const { return int128_holder(lowValue >> y.lowValue); }
    int128_holder operator& (int128_holder y) const { return int128_holder(lowValue &  y.lowValue); }
    int128_holder operator| (int128_holder y) const { return int128_holder(lowValue |  y.lowValue); }
    int128_holder operator^ (int128_holder y) const { return int128_holder(lowValue ^  y.lowValue); }

    operator ptrdiff64_t() const { return lowValue; }
} CLAY_ALIGN(16);

struct uint128_holder {
    size64_t lowValue;
    size64_t highPad; // not used in static math

    uint128_holder() {}
    explicit uint128_holder(size64_t low) : lowValue(low), highPad(0) {}
    uint128_holder(size64_t low, size64_t high) : lowValue(low), highPad(high) {}
    explicit uint128_holder(int128_holder y) : lowValue((size64_t)y.lowValue), highPad((size64_t)y.highPad) {}

    uint128_holder &operator=(size64_t low) {
        new ((void*)this) uint128_holder(low);
        return *this;
    }

    uint128_holder operator-() const { return uint128_holder((size64_t)(-(ptrdiff64_t)lowValue)); }
    uint128_holder operator~() const { return uint128_holder(~lowValue); }

    bool operator==(uint128_holder y) const { return lowValue == y.lowValue; }
    bool operator< (uint128_holder y) const { return lowValue <  y.lowValue; }

    uint128_holder operator+ (uint128_holder y) const { return uint128_holder(lowValue +  y.lowValue); }
    uint128_holder operator- (uint128_holder y) const { return uint128_holder(lowValue -  y.lowValue); }
    uint128_holder operator* (uint128_holder y) const { return uint128_holder(lowValue *  y.lowValue); }
    uint128_holder operator/ (uint128_holder y) const { return uint128_holder(lowValue /  y.lowValue); }
    uint128_holder operator% (uint128_holder y) const { return uint128_holder(lowValue %  y.lowValue); }
    uint128_holder operator<<(uint128_holder y) const { return uint128_holder(lowValue << y.lowValue); }
    uint128_holder operator>>(uint128_holder y) const { return uint128_holder(lowValue >> y.lowValue); }
    uint128_holder operator& (uint128_holder y) const { return uint128_holder(lowValue &  y.lowValue); }
    uint128_holder operator| (uint128_holder y) const { return uint128_holder(lowValue |  y.lowValue); }
    uint128_holder operator^ (uint128_holder y) const { return uint128_holder(lowValue ^  y.lowValue); }

    operator size64_t() const { return lowValue; }
} CLAY_ALIGN(16);

}

namespace std {

template<>
class numeric_limits<clay::int128_holder> {
public:
    static clay::int128_holder min() throw() {
        return clay::int128_holder(0, std::numeric_limits<clay::ptrdiff64_t>::min());
    }
    static clay::int128_holder max() throw() {
        return clay::int128_holder(-1, std::numeric_limits<clay::ptrdiff64_t>::max());
    }
};

template<>
class numeric_limits<clay::uint128_holder> {
public:
    static clay::uint128_holder min() throw() {
        return clay::uint128_holder(0, 0);
    }
    static clay::uint128_holder max() throw() {
        return clay::uint128_holder(
            std::numeric_limits<clay::size64_t>::max(),
            std::numeric_limits<clay::size64_t>::max()
        );
    }
};

}

namespace clay {

inline int128_holder::int128_holder(uint128_holder y)
    : lowValue((ptrdiff64_t)y.lowValue), highPad((ptrdiff64_t)y.highPad) {}

typedef int128_holder clay_int128;
typedef uint128_holder clay_uint128;


}

#endif


namespace clay {

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const clay_int128 &x) {
    return os << ptrdiff64_t(x);
}
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const clay_uint128 &x) {
    return os << size64_t(x);
}

}
