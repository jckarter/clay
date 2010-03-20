
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
#include "llvm/System/Path.h"
#include "llvm/Target/TargetRegistry.h"

#include <iostream>
#include <cstring>

#ifdef WIN32
#define DEFAULT_EXE "a.exe"
#else
#define DEFAULT_EXE "a.out"
#endif

using namespace std;

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

static void generateLLVM(llvm::Module *module, llvm::raw_ostream *out)
{
    llvm::PassManager passes;
    passes.add(llvm::createPrintModulePass(out));
    passes.run(*module);
}

static void generateAssembly(llvm::Module *module, llvm::raw_ostream *out)
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

    llvm::formatted_raw_ostream fout(*out);
    bool result =
        target->addPassesToEmitFile(fpasses,
                                    fout,
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

static bool generateExe(llvm::Module *module,
                        const string &outputFile,
                        const llvm::sys::Path &gccPath)
{
    llvm::sys::Path tempAsm("clayasm");
    string errMsg;
    if (tempAsm.createTemporaryFileOnDisk(false, &errMsg)) {
        cerr << "error: " << errMsg << '\n';
        return false;
    }
    llvm::sys::RemoveFileOnSignal(tempAsm);

    errMsg.clear();
    llvm::raw_fd_ostream asmOut(tempAsm.c_str(),
                                errMsg,
                                llvm::raw_fd_ostream::F_Binary);
    if (!errMsg.empty()) {
        cerr << "error: " << errMsg << '\n';
        return false;
    }

    generateAssembly(module, &asmOut);
    asmOut.close();

    const char *gccArgs[] = {gccPath.c_str(),
                             "-m32",
                             "-o", outputFile.c_str(),
                             "-x", "assembler",
                             tempAsm.c_str(),
                             NULL};
    llvm::sys::Program::ExecuteAndWait(gccPath, gccArgs);

    if (tempAsm.eraseFromDisk(false, &errMsg)) {
        cerr << "error: " << errMsg << '\n';
        return false;
    }
    return true;
}

static void usage()
{
    cerr << "usage: clay <options> <clayfile>\n";
    cerr << "options:\n";
    cerr << "  -o <file>       - specify output file\n";
    cerr << "  -llvm           - emit llvm code\n";
    cerr << "  -asm            - emit assember code\n";
    cerr << "  -unoptimized    - generate unoptimized code\n";
}

int main(int argc, char **argv) {
    if (argc == 1) {
        usage();
        return -1;
    }

    bool optimize = true;
    bool emitLLVM = false;
    bool emitAsm = false;

    string clayFile;
    string outputFile;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-unoptimized") == 0) {
            optimize = false;
        }
        else if (strcmp(argv[i], "-llvm") == 0) {
            emitLLVM = true;
        }
        else if (strcmp(argv[i], "-asm") == 0) {
            emitAsm = true;
        }
        else if (strcmp(argv[i], "-o") == 0) {
            if (i+1 == argc) {
                cerr << "error: filename missing after -o\n";
                return -1;
            }
            if (!outputFile.empty()) {
                cerr << "error: output file already specified: "
                     << outputFile
                     << ", specified again as " << argv[i] << '\n';
                return -1;
            }
            ++i;
            outputFile = argv[i];
        }
        else if (strstr(argv[i], "-") != argv[i]) {
            if (!clayFile.empty()) {
                cerr << "error: clay file already specified: " << clayFile
                     << ", unrecognized parameter: " << argv[i] << '\n';
                return -1;
            }
            clayFile = argv[i];
        }
        else {
            cerr << "error: unrecognized option " << argv[i] << '\n';
            return -1;
        }
    }

    if (clayFile.empty()) {
        cerr << "error: clay file not specified\n";
        return -1;
    }

    if (emitLLVM && emitAsm) {
        cerr << "error: use either -llvm or -asm, not both\n";
        return -1;
    }

    llvm::InitializeAllTargets();
    llvm::InitializeAllAsmPrinters();
    initLLVM();
    llvmModule->setTargetTriple(llvm::sys::getHostTriple());

    initTypes();

    llvm::sys::Path clayExe =
        llvm::sys::Path::GetMainExecutable(argv[0], (void *)&main);
    llvm::sys::Path clayDir(clayExe.getDirname());
    llvm::sys::Path libDir(clayDir);
    bool result = libDir.appendComponent("lib-clay");
    assert(result);
    addSearchPath(libDir.str());

    if (outputFile.empty()) {
        if (emitLLVM)
            outputFile = "a.ll";
        else if (emitAsm)
            outputFile = "a.s";
        else
            outputFile = DEFAULT_EXE;
    }
    llvm::sys::RemoveFileOnSignal(llvm::sys::Path(outputFile));

    ModulePtr m = loadProgram(clayFile);
    codegenMain(m);

    if (optimize)
        optimizeLLVM(llvmModule);

    if (emitLLVM || emitAsm) {
        string errorInfo;
        llvm::raw_fd_ostream out(outputFile.c_str(),
                                 errorInfo,
                                 llvm::raw_fd_ostream::F_Binary);
        if (!errorInfo.empty()) {
            cerr << "error: " << errorInfo << '\n';
            return -1;
        }
        if (emitLLVM)
            generateLLVM(llvmModule, &out);
        else if (emitAsm)
            generateAssembly(llvmModule, &out);
    }
    else {
        bool result;
        llvm::sys::Path gccPath;
#ifdef WIN32
        gccPath = clayDir;
        result = gccPath.appendComponent("mingw");
        assert(result);
        result = gccPath.appendComponent("bin");
        assert(result);
        result = gccPath.appendComponent("gcc.exe");
        assert(result);
        if (!gccPath.exists())
            gccPath = llvm::sys::Path();
#endif
        if (!gccPath.isValid()) {
            gccPath = llvm::sys::Program::FindProgramByName("gcc");
        }
        if (!gccPath.isValid()) {
            cerr << "error: unable to find gcc on the path\n";
            return false;
        }

        result = generateExe(llvmModule, outputFile, gccPath);
        if (!result)
            return -1;
    }

    return 0;
}
