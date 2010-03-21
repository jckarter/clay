#include "clay.hpp"
#include "llvm/System/Host.h"
#include <llvm/Target/TargetSelect.h>

llvm::Module *llvmModule;
llvm::ExecutionEngine *llvmEngine;
const llvm::TargetData *llvmTargetData;

llvm::Function *llvmFunction;
llvm::IRBuilder<> *llvmInitBuilder;
llvm::IRBuilder<> *llvmBuilder;

void initLLVM() {
    llvm::InitializeAllTargets();
    llvm::InitializeAllAsmPrinters();
    llvmModule = new llvm::Module("clay", llvm::getGlobalContext());
    llvm::EngineBuilder eb(llvmModule);
    llvmEngine = eb.create();
    assert(llvmEngine);
    llvmTargetData = llvmEngine->getTargetData();
    llvmModule->setDataLayout(llvmTargetData->getStringRepresentation());
    llvmModule->setTargetTriple(llvm::sys::getHostTriple());

    llvmFunction = NULL;
    llvmInitBuilder = NULL;
    llvmBuilder = NULL;
}

llvm::BasicBlock *newBasicBlock(const char *name)
{
    return llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                    name,
                                    llvmFunction);
}
