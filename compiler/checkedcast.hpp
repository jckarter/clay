#pragma once


#include "assert.h"


namespace clay {


template <class To, class From>
To checked_cast(From* from) {
    // TODO: for unknown reason this code cause SEGV in runtime
    //assert(!from || dynamic_cast<To>(from));
    return static_cast<To>(from);
}


}
