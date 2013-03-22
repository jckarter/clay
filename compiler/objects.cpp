#include "clay.hpp"
#include "objects.hpp"
#include "types.hpp"

namespace clay {


//
// ValueHolder constructor and destructor
//

ValueHolder::ValueHolder(TypePtr type)
    : Object(VALUE_HOLDER), type(type)
{
    this->buf = (char *)calloc(1, typeSize(type));
}

ValueHolder::~ValueHolder()
{
    // FIXME: call clay 'destroy'
    free(this->buf);
}



//
// objectEquals
//

// FIXME: this doesn't handle arbitrary values (need to call clay)
bool _objectValueEquals(ObjectPtr a, ObjectPtr b)
{
    // at this point pointer identity should already have been checked by objectEquals

    int aKind = a->objKind, bKind = b->objKind;
    if (aKind != bKind)
        return false;
    
    switch (aKind) {
        
    case IDENTIFIER : {
        Identifier *a1 = (Identifier *)a.ptr();
        Identifier *b1 = (Identifier *)b.ptr();
        return a1->str == b1->str;
    }

    case VALUE_HOLDER : {
        ValueHolder *a1 = (ValueHolder *)a.ptr();
        ValueHolder *b1 = (ValueHolder *)b.ptr();
        if (a1->type != b1->type)
            return false;
        size_t n = typeSize(a1->type);
        return memcmp(a1->buf, b1->buf, n) == 0;
    }

    case PVALUE : {
        PValue *a1 = (PValue *)a.ptr();
        PValue *b1 = (PValue *)b.ptr();
        return a1->data == b1->data;
    }

    default :
        return false;
    }
}



//
// objectHash
//

static unsigned identityHash(Object *a)
{
    size_t v = (size_t)a;
    return unsigned(int(v));
}

// FIXME: this doesn't handle arbitrary values (need to call clay)
unsigned objectHash(ObjectPtr a)
{
    switch (a->objKind) {

    case IDENTIFIER : {
        Identifier *b = (Identifier *)a.ptr();
        unsigned h = 0;
        for (unsigned i = 0; i < b->str.size(); ++i)
            h += unsigned(b->str[i]);
        return h;
    }

    case VALUE_HOLDER : {
        ValueHolder *b = (ValueHolder *)a.ptr();
        // TODO: call clay 'hash'
        unsigned h = 0;
        size_t n = typeSize(b->type);
        for (unsigned i = 0; i < n; ++i)
            h += unsigned(b->buf[i]);
        h = h*11 + identityHash(b->type.ptr());
        return h;
    }

    case PVALUE : {
        PValue *b = (PValue *)a.ptr();
        unsigned h = identityHash(b->data.type.ptr());
        h *= b->data.isTemp ? 11 : 23;
        return h;
    }

    default : {
        return identityHash(a.ptr());
    }

    }
}



//
// ObjectTable
//

Pointer<RefCounted> &ObjectTable::lookup(llvm::ArrayRef<ObjectPtr> key)
{
    if (this->buckets.empty())
        this->buckets.resize(16);
    if (this->buckets.size() < 2*this->size)
        rehash();
    unsigned h = objectVectorHash(key);
    h &= unsigned(buckets.size() - 1);
    vector<ObjectTableNode> &bucket = buckets[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        if (objectVectorEquals(key, bucket[i].key))
            return bucket[i].value;
    }
    bucket.push_back(ObjectTableNode(key, NULL));
    ++this->size;
    return bucket.back().value;
}

void ObjectTable::rehash()
{
    vector< vector<ObjectTableNode> > newBuckets;
    newBuckets.resize(4 * this->buckets.size());

    for (unsigned i = 0; i < this->buckets.size(); ++i) {
        vector<ObjectTableNode> &bucket = this->buckets[i];
        for (unsigned j = 0; j < bucket.size(); ++j) {
            unsigned h = objectVectorHash(bucket[j].key);
            h &= unsigned(newBuckets.size() - 1);
            newBuckets[h].push_back(bucket[j]);
        }
    }

    this->buckets = newBuckets;
}

}
