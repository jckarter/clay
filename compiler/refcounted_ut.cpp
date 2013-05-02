#include "refcounted.hpp"

#include "ut.hpp"


namespace clay {

CLAY_UNITTEST(RefCounted_copy_constructor) {
    Pointer<RefCounted> p(new RefCounted);
    UT_ASSERT(p->getRefCount() == 1);

    Pointer<RefCounted> p2(new RefCounted(*p));
    UT_ASSERT(p->getRefCount() == 1);
    UT_ASSERT(p2->getRefCount() == 1);
}

}
