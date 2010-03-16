
#include "clay.hpp"

#include "llvm/PassManager.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/LinkAllVMCore.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/StandardPasses.h"
#include "llvm/System/Host.h"
#include "llvm/Target/TargetRegistry.h"

#include <iostream>
#include <cstring>

using namespace std;

static void setSearchPath(const char *exePath)
{
    const char *end = exePath + strlen(exePath);
    while (end != exePath) {
        if ((end[-1] == '\\') || (end[-1] == '/'))
            break;
        --end;
    }
    string dirPath(exePath, end);
    addSearchPath(dirPath + "lib-clay");
}

static void addOptimizationPasses(llvm::PassManager &passes,
                                  llvm::FunctionPassManager &fpasses,
                                  unsigned optLevel)
{
    llvm::createStandardFunctionPasses(&fpasses, optLevel);

    llvm::Pass *inliningPass = 0;
    if (optLevel) {
        unsigned threshold = 200;
        if (optLevel > 2)
            threshold = 250;
        inliningPass = llvm::createFunctionInliningPass(threshold);
    } else {
        inliningPass = llvm::createAlwaysInlinerPass();
    }
    llvm::createStandardModulePasses(&passes,
                                     optLevel,
                                     /*OptimizeSize=*/ false,
                                     /*UnitAtATime=*/ false,
                                     /*UnrollLoops=*/ optLevel > 1,
                                     /*SimplifyLibCalls=*/ true,
                                     /*HaveExceptions=*/ true,
                                     inliningPass);
    llvm::createStandardLTOPasses(&passes,
                                  /*Internalize=*/ true,
                                  /*RunInliner=*/ true,
                                  /*VerifyEach=*/ false);
}

static void optimizeLLVM(llvm::Module *module)
{
    llvm::PassManager passes;

    string moduleDataLayout = module->getDataLayout();
    llvm::TargetData *td = new llvm::TargetData(moduleDataLayout);
    passes.add(td);

    llvm::FunctionPassManager fpasses(module);

    fpasses.add(new llvm::TargetData(*td));

    addOptimizationPasses(passes, fpasses, 3);

    fpasses.doInitialization();
    for (llvm::Module::iterator i = module->begin(), e = module->end();
         i != e; ++i)
    {
        fpasses.run(*i);
    }

    passes.add(llvm::createVerifierPass());

    passes.run(*module);
}

static void generateLLVMAssembly(llvm::Module *module)
{
    llvm::PassManager passes;
    passes.add(llvm::createPrintModulePass(&llvm::outs()));
    passes.run(*module);
}

static void generateNativeAssembly(llvm::Module *module)
{
    llvm::Triple theTriple(module->getTargetTriple());
    string err;
    const llvm::Target *theTarget =
        llvm::TargetRegistry::lookupTarget(theTriple.getTriple(), err);
    assert(theTarget != NULL);
    llvm::TargetMachine *target =
        theTarget->createTargetMachine(theTriple.getTriple(), "");
    assert(target != NULL);

    llvm::CodeGenOpt::Level optLevel = llvm::CodeGenOpt::Aggressive;

    llvm::FunctionPassManager fpasses(module);

    fpasses.add(new llvm::TargetData(module));
    fpasses.add(llvm::createVerifierPass());

    target->setAsmVerbosityDefault(true);

    bool result =
        target->addPassesToEmitFile(fpasses,
                                    llvm::fouts(),
                                    llvm::TargetMachine::CGFT_AssemblyFile,
                                    optLevel);
    assert(!result);

    fpasses.doInitialization();
    for (llvm::Module::iterator i = module->begin(), e = module->end();
         i != e; ++i)
    {
        fpasses.run(*i);
    }
    fpasses.doFinalization();
}

static void usage()
{
    cerr << "usage: clay <options> <clayfile>\n";
    cerr << "options:\n";
    cerr << "  --unoptimized   - generate unoptimized code\n";
    cerr << "  --emit-llvm     - emit llvm assembly language\n";
    cerr << "  --emit-native   - emit native assembly language\n";
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return -1;
    }

    bool optimize = true;
    bool emitLLVM = false;
    bool emitNative = false;

    for (int i = 1; i+1 < argc; ++i) {
        if (strcmp(argv[i], "--unoptimized") == 0)
            optimize = false;
        else if (strcmp(argv[i], "--emit-llvm") == 0)
            emitLLVM = true;
        else if (strcmp(argv[i], "--emit-native") == 0)
            emitNative = true;
        else
            cerr << "unrecognized option: " << argv[i] << '\n';
    }

    if (emitLLVM && emitNative) {
        cerr << "use either --emit-llvm or --emit-native, not both\n";
        return -1;
    }
    if (!emitLLVM && !emitNative)
        emitNative = true;

    const char *clayFile = argv[argc-1];

    llvm::InitializeAllTargets();
    llvm::InitializeAllAsmPrinters();
    initLLVM();
    llvmModule->setTargetTriple(llvm::sys::getHostTriple());

    initTypes();

    setSearchPath(argv[0]);

    ModulePtr m = loadProgram(clayFile);
    codegenMain(m);

    if (optimize)
        optimizeLLVM(llvmModule);

    if (emitLLVM)
        generateLLVMAssembly(llvmModule);
    else if (emitNative)
        generateNativeAssembly(llvmModule);

    return 0;
}
