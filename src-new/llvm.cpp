#include "clay.hpp"

llvm::Module *llvmModule;
llvm::ExecutionEngine *llvmEngine;
const llvm::TargetData *llvmTargetData;

llvm::Function *llvmFunction;
llvm::IRBuilder<> *llvmInitBuilder;
llvm::IRBuilder<> *llvmBuilder;

void initLLVM() {
    llvm::InitializeNativeTarget();
    llvmModule = new llvm::Module("clay", llvm::getGlobalContext());
    llvm::EngineBuilder eb(llvmModule);
    llvmEngine = eb.create();
    assert(llvmEngine);
    llvmTargetData = llvmEngine->getTargetData();
    llvmModule->setDataLayout(llvmTargetData->getStringRepresentation());

    llvmFunction = NULL;
    llvmInitBuilder = NULL;
    llvmBuilder = NULL;
}
