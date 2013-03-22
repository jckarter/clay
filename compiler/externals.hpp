#ifndef __EXTERNALS_HPP
#define __EXTERNALS_HPP

#include "clay.hpp"
#include "codegen.hpp"


namespace clay {

struct ExtArgInfo {
    enum Kind {
        Direct, Indirect
    };

    Kind kind;
    llvm::Type *type;

    ExtArgInfo(enum Kind k = Direct, llvm::Type *ll = NULL)
        : kind(k), type(ll) {}

    static ExtArgInfo getDirect(llvm::Type* ll = NULL) {
        return ExtArgInfo(Direct, ll);
    }
    static ExtArgInfo getIndirect() {
        return ExtArgInfo(Indirect, NULL);
    }

    bool isDirect() const { return kind == Direct; }
    bool isIndirect() const { return kind == Indirect; }
};

struct ExternalFunction : public llvm::FoldingSetNode {
    struct ArgInfo {
        TypePtr type;
        ExtArgInfo ext;
    };

    CallingConv conv;
    size_t numReqArg;
    bool isVarArg;

    llvm::CallingConv::ID llConv;
    ArgInfo retInfo;
    vector<ArgInfo> argInfos;
    vector< pair<unsigned, llvm::Attributes> > attrs;

    ExternalFunction(CallingConv conv, TypePtr ret,
                     vector<TypePtr> &args, size_t numReqArg,
                     bool isVarArg) {
        this->conv = conv;
        this->retInfo.type = ret;
        for (vector<TypePtr>::iterator it = args.begin(), ie = args.end();
            it != ie; ++it)
        {
            ArgInfo info = { *it, ExtArgInfo::getDirect() };
            this->argInfos.push_back(info);
        }
        this->numReqArg = numReqArg;
        this->isVarArg = isVarArg;
    }

    static void Profile(llvm::FoldingSetNodeID &ID,
                        CallingConv conv, TypePtr ret,
                        vector<TypePtr> &args, size_t numReqArg,
                        bool isVarArg) {
        ID.AddInteger(conv);
        ID.AddPointer(ret.ptr());
        for (vector<TypePtr>::iterator it = args.begin(), ie = args.end();
            it != ie; ++it)
        {
            ID.AddPointer(it->ptr());
        }
        ID.AddInteger(numReqArg);
        ID.AddBoolean(isVarArg);
    }

    void Profile(llvm::FoldingSetNodeID &ID) {
        ID.AddInteger(conv);
        ID.AddPointer(retInfo.type.ptr());
        for (vector<ArgInfo>::iterator
            it = argInfos.begin(), ie = argInfos.end();
            it != ie; ++it)
        {
            ID.AddPointer(it->type.ptr());
        }
        ID.AddInteger(numReqArg);
        ID.AddBoolean(isVarArg);
    }

    llvm::FunctionType* getLLVMFunctionType() const;

    // for generating C function definitions
    void allocReturnValue(ArgInfo info,
                          llvm::Function::arg_iterator &ai,
                          vector<CReturn> &returns,
                          CodegenContext* ctx);
    CValuePtr allocArgumentValue(ArgInfo info,
                                 llvm::StringRef name,
                                 llvm::Function::arg_iterator &ai,
                                 CodegenContext* ctx);
    void returnStatement(ArgInfo info,
                         vector<CReturn> &returns,
                         CodegenContext* ctx);

    // for calling C functions
    void loadStructRetArgument(ArgInfo info,
                               vector<llvm::Value *> &llArgs,
                               CodegenContext* ctx,
                               MultiCValuePtr out);
    void loadArgument(ArgInfo info,
                      CValuePtr cv,
                      vector<llvm::Value *> &llArgs,
                      CodegenContext* ctx,
                      bool isVarArg);
    void storeReturnValue(ArgInfo info,
                          llvm::Value *callReturnValue,
                          CodegenContext* ctx,
                          MultiCValuePtr out);
};

struct ExternalTarget : public RefCounted {
    llvm::FoldingSet<ExternalFunction> extFuncs;

    ExternalTarget() {}
    virtual ~ExternalTarget() {}

    ExternalFunction* getExternalFunction(
        CallingConv conv,
        TypePtr ret,
        vector<TypePtr> &args,
        size_t numReqArg, bool varArg);

    virtual void computeInfo(ExternalFunction *f) = 0;
};

typedef Pointer<ExternalTarget> ExternalTargetPtr;

ExternalTargetPtr getExternalTarget();


}

#endif // __EXTERNALS_HPP
