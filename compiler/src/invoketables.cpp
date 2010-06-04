#include "clay.hpp"



//
// callableOverloads
//

static void initCallable(ObjectPtr x)
{
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        if (!y->overloadsInitialized)
            initTypeOverloads(y);
        break;
    }
    case RECORD : {
        Record *y = (Record *)x.ptr();
        if (!y->builtinOverloadInitialized)
            initBuiltinConstructor(y);
        break;
    }
    case PROCEDURE :
        break;
    default :
        assert(false);
    }
}

const vector<OverloadPtr> &callableOverloads(ObjectPtr x)
{
    initCallable(x);
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        return y->overloads;
    }
    case RECORD : {
        Record *y = (Record *)x.ptr();
        return y->overloads;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        return y->overloads;
    }
    default : {
        assert(false);
        const vector<OverloadPtr> *ptr = NULL;
        return *ptr;
    }
    }
}



//
// invoke tables
//

static bool invokeTablesInitialized = false;

static vector< vector<InvokeSetPtr> > invokeTable;


static void initInvokeTables() {
    assert(!invokeTablesInitialized);
    invokeTable.resize(16384);
    invokeTablesInitialized = true;
}



//
// lookupInvokeSet
//

InvokeSetPtr lookupInvokeSet(ObjectPtr callable,
                             const vector<TypePtr> &argsKey)
{
    if (!invokeTablesInitialized)
        initInvokeTables();
    int h = objectHash(callable) + objectVectorHash(argsKey);
    h &= (invokeTable.size() - 1);
    vector<InvokeSetPtr> &bucket = invokeTable[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        InvokeSetPtr invokeSet = bucket[i];
        if (objectEquals(invokeSet->callable, callable) &&
            objectVectorEquals(invokeSet->argsKey, argsKey))
        {
            return invokeSet;
        }
    }
    InvokeSetPtr invokeSet = new InvokeSet(callable, argsKey);
    bucket.push_back(invokeSet);
    return invokeSet;
}
