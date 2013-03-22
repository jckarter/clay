#ifndef __OBJECTS_HPP
#define __OBJECTS_HPP


#include "clay.hpp"


namespace clay {

struct ValueHolder : public Object {
    TypePtr type;
    char *buf;
    ValueHolder(TypePtr type);
    ~ValueHolder();

    template<typename T>
    T &as() { return *(T*)buf; }

    template<typename T>
    T const &as() const { return *(T const *)buf; }
};


bool _objectValueEquals(ObjectPtr a, ObjectPtr b);
unsigned objectHash(ObjectPtr a);

inline bool objectEquals(ObjectPtr a, ObjectPtr b) {
    if (a == b)
        return true;
    return _objectValueEquals(a,b);
}

template <typename ObjectVectorA, typename ObjectVectorB>
inline bool objectVectorEquals(ObjectVectorA const &a,
                        ObjectVectorB const &b) {
    if (a.size() != b.size()) return false;
    for (unsigned i = 0; i < a.size(); ++i) {
        if (!objectEquals(a[i].ptr(), b[i].ptr()))
            return false;
    }
    return true;
}

template <typename ObjectVector>
inline unsigned objectVectorHash(ObjectVector const &a) {
    unsigned h = 0;
    for (unsigned i = 0; i < a.size(); ++i)
        h += objectHash(a[i].ptr());
    return h;
}

struct ObjectTableNode {
    vector<ObjectPtr> key;
    Pointer<RefCounted> value;
    ObjectTableNode(llvm::ArrayRef<ObjectPtr> key,
                    Pointer<RefCounted> value)
        : key(key), value(value) {}
};

struct ObjectTable : public RefCounted {
    vector< vector<ObjectTableNode> > buckets;
    unsigned size;
public :
    ObjectTable() : size(0) {}
    Pointer<RefCounted> &lookup(llvm::ArrayRef<ObjectPtr> key);
private :
    void rehash();
};

} // namespace clay


#endif // __OBJECTS_HPP
