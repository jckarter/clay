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
int objectHash(ObjectPtr a);

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
inline int objectVectorHash(ObjectVector const &a) {
    int h = 0;
    for (unsigned i = 0; i < a.size(); ++i)
        h += objectHash(a[i].ptr());
    return h;
}

struct ObjectTableNode {
    vector<ObjectPtr> key;
    ObjectPtr value;
    ObjectTableNode(llvm::ArrayRef<ObjectPtr> key,
                    ObjectPtr value)
        : key(key), value(value) {}
};

struct ObjectTable : public Object {
    vector< vector<ObjectTableNode> > buckets;
    unsigned size;
public :
    ObjectTable() : Object(DONT_CARE), size(0) {}
    ObjectPtr &lookup(llvm::ArrayRef<ObjectPtr> key);
private :
    void rehash();
};

} // namespace clay


#endif // __OBJECTS_HPP
