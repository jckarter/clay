#ifndef __EXTERNALS_HPP
#define __EXTERNALS_HPP

#include "clay.hpp"
#include "codegen.hpp"


namespace clay {

struct ExternalTarget : public Object {
    ExternalTarget() : Object(DONT_CARE) {}
    virtual ~ExternalTarget() {}

    virtual llvm::CallingConv::ID callingConvention(CallingConv conv) = 0;

    virtual llvm::Type *typeReturnsAsBitcastType(CallingConv conv, TypePtr type) = 0;
    virtual llvm::Type *typePassesAsBitcastType(CallingConv conv, TypePtr type, bool varArg) = 0;
    virtual bool typeReturnsBySretPointer(CallingConv conv, TypePtr type) = 0;
    virtual bool typePassesByByvalPointer(CallingConv conv, TypePtr type, bool varArg) = 0;

    // for generating C function declarations
    llvm::Type *pushReturnType(CallingConv conv,
                               TypePtr type,
                               vector<llvm::Type *> &llArgTypes,
                               vector< pair<unsigned, llvm::Attributes> > &llAttributes);
    void pushArgumentType(CallingConv conv,
                          TypePtr type,
                          vector<llvm::Type *> &llArgTypes,
                          vector< pair<unsigned, llvm::Attributes> > &llAttributes);

    // for generating C function definitions
    void allocReturnValue(CallingConv conv,
                          TypePtr type,
                          llvm::Function::arg_iterator &ai,
                          vector<CReturn> &returns,
                          CodegenContext* ctx);
    CValuePtr allocArgumentValue(CallingConv conv,
                                 TypePtr type,
                                 llvm::StringRef name,
                                 llvm::Function::arg_iterator &ai,
                                 CodegenContext* ctx);
    void returnStatement(CallingConv conv,
                         TypePtr type,
                         vector<CReturn> &returns,
                         CodegenContext* ctx);

    // for calling C functions
    void loadStructRetArgument(CallingConv conv,
                               TypePtr type,
                               vector<llvm::Value *> &llArgs,
                               vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                               CodegenContext* ctx,
                               MultiCValuePtr out);
    void loadArgument(CallingConv conv,
                      CValuePtr cv,
                      vector<llvm::Value *> &llArgs,
                      vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                      CodegenContext* ctx);
    void loadVarArgument(CallingConv conv,
                         CValuePtr cv,
                         vector<llvm::Value *> &llArgs,
                         vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                         CodegenContext* ctx);
    void storeReturnValue(CallingConv conv,
                          llvm::Value *callReturnValue,
                          TypePtr returnType,
                          CodegenContext* ctx,
                          MultiCValuePtr out);
};

typedef Pointer<ExternalTarget> ExternalTargetPtr;

ExternalTargetPtr getExternalTarget(CompilerState* cst);


}

#endif // __EXTERNALS_HPP
