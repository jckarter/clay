
#include "clay.hpp"
#include "hirestimer.hpp"

#include "llvm/PassManager.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/LinkAllVMCore.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/StandardPasses.h"
#include "llvm/Support/Path.h"
#include "llvm/Target/TargetRegistry.h"
#include "llvm/Target/TargetOptions.h"

#include <iostream>
#include <cstring>

#ifdef WIN32
#define DEFAULT_EXE "a.exe"
#define DEFAULT_DLL "a.dll"
#define PATH_SEPARATORS "/\\"
#else
#define DEFAULT_EXE "a.out"
#define DEFAULT_DLL "a.out"
#define PATH_SEPARATORS "/"
#endif

#ifdef WIN32
#define ENV_SEPARATOR ';'
#else
#define ENV_SEPARATOR ':'
#endif

using namespace std;

static void addOptimizationPasses(llvm::PassManager &passes,
                                  llvm::FunctionPassManager &fpasses,
                                  unsigned optLevel,
                                  bool internalize)
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
                                     /*UnitAtATime=*/ true,
                                     /*UnrollLoops=*/ optLevel > 1,
                                     /*SimplifyLibCalls=*/ true,
                                     /*HaveExceptions=*/ true,
                                     inliningPass);
    llvm::createStandardLTOPasses(&passes,
                                  /*Internalize=*/ internalize,
                                  /*RunInliner=*/ true,
                                  /*VerifyEach=*/ false);
}

static void runModule(llvm::Module *module)
{
    llvm::EngineBuilder eb(llvmModule);
    llvm::ExecutionEngine *engine = eb.create();
    llvm::Function *mainFunc = module->getFunction("main");
    assert(mainFunc);
    llvm::Function *globalCtors = 
        module->getFunction("clayglobals_init");
    llvm::Function *globalDtors = 
        module->getFunction("clayglobals_destroy");

    engine->runFunction(globalCtors, vector<llvm::GenericValue>());

    vector<llvm::GenericValue> gvArgs;
    llvm::GenericValue gv;
    gv.IntVal = llvm::APInt(32, 0, true);
    gvArgs.push_back(gv);
    gvArgs.push_back(llvm::GenericValue((char *)NULL));
    engine->runFunction(mainFunc, gvArgs);

    engine->runFunction(globalDtors, vector<llvm::GenericValue>());

    delete engine;
}

static void optimizeLLVM(llvm::Module *module, unsigned optLevel, bool internalize)
{
    llvm::PassManager passes;

    string moduleDataLayout = module->getDataLayout();
    llvm::TargetData *td = new llvm::TargetData(moduleDataLayout);
    passes.add(td);

    llvm::FunctionPassManager fpasses(module);

    fpasses.add(new llvm::TargetData(*td));

    addOptimizationPasses(passes, fpasses, optLevel, internalize);

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

static void generateAssembly(llvm::Module *module,
                             llvm::raw_ostream *out,
                             bool emitObject,
                             unsigned optLevel,
                             bool sharedLib,
                             bool genPIC)
{
    llvm::Triple theTriple(module->getTargetTriple());
    string err;
    const llvm::Target *theTarget =
        llvm::TargetRegistry::lookupTarget(theTriple.getTriple(), err);
    assert(theTarget != NULL);
    if (optLevel < 2)
        llvm::NoFramePointerElim = true;
    if (sharedLib || genPIC)
        llvm::TargetMachine::setRelocationModel(llvm::Reloc::PIC_);
    llvm::TargetMachine::setCodeModel(llvm::CodeModel::Default);
    llvm::TargetMachine *target =
        theTarget->createTargetMachine(theTriple.getTriple(), "");
    assert(target != NULL);

    llvm::FunctionPassManager fpasses(module);

    fpasses.add(new llvm::TargetData(module));
    fpasses.add(llvm::createVerifierPass());

    target->setAsmVerbosityDefault(true);

    llvm::formatted_raw_ostream fout(*out);
    llvm::TargetMachine::CodeGenFileType fileType = emitObject
        ? llvm::TargetMachine::CGFT_ObjectFile
        : llvm::TargetMachine::CGFT_AssemblyFile;

    llvm::CodeGenOpt::Level level;
    switch (optLevel) {
    case 0 : level = llvm::CodeGenOpt::None; break;
    case 1 : level = llvm::CodeGenOpt::Less; break;
    case 2 : level = llvm::CodeGenOpt::Default; break;
    default : level = llvm::CodeGenOpt::Aggressive; break;
    }
    bool result =
        target->addPassesToEmitFile(fpasses,
                                    fout,
                                    fileType,
                                    level);
    assert(!result);

    fpasses.doInitialization();
    for (llvm::Module::iterator i = module->begin(), e = module->end();
         i != e; ++i)
    {
        fpasses.run(*i);
    }
    fpasses.doFinalization();
}

static bool generateBinary(llvm::Module *module,
                           const string &outputFile,
                           const llvm::sys::Path &gccPath,
                           unsigned optLevel,
                           bool /*exceptions*/,
                           bool sharedLib,
                           bool genPIC,
                           const vector<string> &arguments)
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

    generateAssembly(module, &asmOut, false, optLevel, sharedLib, genPIC);
    asmOut.close();

    vector<const char *> gccArgs;
    gccArgs.push_back(gccPath.c_str());

    switch (llvmTargetData->getPointerSizeInBits()) {
    case 32 :
        gccArgs.push_back("-m32");
        break;
    case 64 :
        gccArgs.push_back("-m64");
        break;
    default :
        assert(false);
    }

    if (sharedLib)
        gccArgs.push_back("-shared");
    gccArgs.push_back("-o");
    gccArgs.push_back(outputFile.c_str());
    gccArgs.push_back("-x");
    gccArgs.push_back("assembler");
    gccArgs.push_back(tempAsm.c_str());
    for (unsigned i = 0; i < arguments.size(); ++i)
        gccArgs.push_back(arguments[i].c_str());
    gccArgs.push_back(NULL);

    int result = llvm::sys::Program::ExecuteAndWait(gccPath, &gccArgs[0]);

    if (tempAsm.eraseFromDisk(false, &errMsg)) {
        cerr << "error: " << errMsg << '\n';
        return false;
    }

    return (result == 0);
}

static void usage(char *argv0)
{
    cerr << "usage: " << argv0 << " <options> <clayfile>\n";
    cerr << "       " << argv0 << " <options> -e <clay source>\n";
    cerr << "options:\n";
    cerr << "  -o <file>         - specify output file\n";
    cerr << "  -target <target>  - set target platform for code generation\n";
    cerr << "  -shared           - create a dynamically linkable library\n";
    cerr << "  -dll              - create a dynamically linkable library\n";
    cerr << "  -llvm             - emit llvm code\n";
    cerr << "  -asm              - emit assember code\n";
    cerr << "  -c                - emit object code\n";
    cerr << "  -unoptimized      - generate unoptimized code\n";
    cerr << "  -exceptions       - enable exception handling\n";
    cerr << "  -no-exceptions    - disable exception handling\n";
    cerr << "  -inline           - inline procedures marked 'inline'\n";
    cerr << "  -no-inline        - ignore 'inline' keyword\n";
    cerr << "  -pic              - generate position independent code\n";
    cerr << "  -abort            - abort on error (to get stacktrace in gdb)\n";
    cerr << "  -run              - execute the program without writing to disk\n";
#if !(defined(_WIN32) || defined(_WIN64))
    cerr << "  -timing           - show timing information\n";
#endif
#ifdef __APPLE__
    cerr << "  -arch <arch>      - build for Darwin architecture <arch>\n";
    cerr << "  -F<dir>           - add <dir> to framework search path\n";
    cerr << "  -framework <name> - link with framework <name>\n";
#endif
    cerr << "  -L<dir>           - add <dir> to library search path\n";
    cerr << "  -l<lib>           - link with library <lib>\n";
    cerr << "  -I<path>          - add <path> to clay module search path\n";
    cerr << "  -e <source>       - compile and run <source> (implies -run)\n";
    cerr << "  -M<module>        - \"import <module>.*;\" for -e\n";
    cerr << "  -v                - display version info\n";
}

static string basename(const string &fullname)
{
    string::size_type to = fullname.rfind('.');
    string::size_type slash = fullname.find_last_of(PATH_SEPARATORS);
    string::size_type from = slash == string::npos ? 0 : slash+1;
    string::size_type length = to == string::npos ? string::npos : to - from;

    return fullname.substr(from, length);
}

int main(int argc, char **argv) {
    if (argc == 1) {
        usage(argv[0]);
        return -1;
    }

    bool optimize = true;
    bool emitLLVM = false;
    bool emitAsm = false;
    bool emitObject = false;
    bool sharedLib = false;
    bool genPIC = false;
    bool inlineEnabled = true;
    bool exceptions = true;
    bool abortOnError = false;
    bool run = false;
    bool crossCompiling = false;
    bool showTiming = false;

#ifdef __APPLE__
    genPIC = true;

    string arch;
#endif

    string clayFile;
    string outputFile;
    string targetTriple = llvm::sys::getHostTriple();

    string clayScriptImports;
    string clayScript;

    vector<string> libSearchPath;
    vector<string> libraries;
#ifdef __APPLE__
    vector<string> frameworkSearchPath;
    vector<string> frameworks;
#endif

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-shared") == 0) ||
            (strcmp(argv[i], "-dll") == 0))
        {
            sharedLib = true;
        }
        else if (strcmp(argv[i], "-llvm") == 0) {
            emitLLVM = true;
        }
        else if (strcmp(argv[i], "-asm") == 0) {
            emitAsm = true;
        }
        else if (strcmp(argv[i], "-c") == 0) {
            emitObject = true;
        }
        else if (strcmp(argv[i], "-unoptimized") == 0) {
            optimize = false;
        }
        else if (strcmp(argv[i], "-inline") == 0) {
            inlineEnabled = true;
        }
        else if (strcmp(argv[i], "-no-inline") == 0) {
            inlineEnabled = false;
        }
        else if (strcmp(argv[i], "-exceptions") == 0) {
            exceptions = true;
        }
        else if (strcmp(argv[i], "-no-exceptions") == 0) {
            exceptions = false;
        }
        else if (strcmp(argv[i], "-pic") == 0) {
            genPIC = true;
        }
        else if (strcmp(argv[i], "-abort") == 0) {
            abortOnError = true;
        }
        else if (strcmp(argv[i], "-run") == 0) {
            run = true;
        }
        else if (strcmp(argv[i], "-timing") == 0) {
            showTiming = true;
        }
        else if (strcmp(argv[i], "-e") == 0) {
            if (i+1 == argc) {
                cerr << "error: source string missing after -e\n";
                return -1;
            }
            ++i;
            run = true;
            clayScript += argv[i];
            clayScript += "\n";
        }
        else if (strncmp(argv[i], "-M", 2) == 0) {
            string modulespec = argv[i]+2; 
            if (modulespec.empty()) {
                cerr << "error: module missing after -M\n";
                return -1;
            }
            clayScriptImports += "import " + modulespec + ".*; ";
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
#ifdef __APPLE__
        else if (strstr(argv[i], "-F") == argv[i]) {
            string frameworkDir = argv[i] + strlen("-F");
            if (frameworkDir.empty()) {
                if (i+1 == argc) {
                    cerr << "error: directory missing after -F\n";
                    return -1;
                }
                ++i;
                frameworkDir = argv[i];
                if (frameworkDir.empty() || (frameworkDir[0] == '-')) {
                    cerr << "error: directory missing after -F\n";
                    return -1;
                }
            }
            frameworkSearchPath.push_back("-F" + frameworkDir);
        }
        else if (strcmp(argv[i], "-framework") == 0) {
            if (i+1 == argc) {
                cerr << "error: framework name missing after -framework\n";
                return -1;
            }
            ++i;
            string framework = argv[i];
            if (framework.empty() || (framework[0] == '-')) {
                cerr << "error: framework name missing after -framework\n";
                return -1;
            }
            frameworks.push_back("-framework");
            frameworks.push_back(framework);
        }
        else if (strcmp(argv[i], "-arch") == 0) {
            if (i+1 == argc) {
                cerr << "error: architecture name missing after -arch\n";
                return -1;
            }
            ++i;
            if (!arch.empty()) {
                cerr << "error: multiple -arch flags currently unsupported\n";
                return -1;
            }
            arch = argv[i];
            if (arch.empty() || (arch[0] == '-')) {
                cerr << "error: architecture name missing after -arch\n";
                return -1;
            }

            if (arch == "i386") {
                targetTriple = "i386-apple-darwin10";
            } else if (arch == "x86_64") {
                targetTriple = "x86_64-apple-darwin10";
            } else if (arch == "ppc") {
                targetTriple = "powerpc-apple-darwin10";
            } else if (arch == "ppc64") {
                targetTriple = "powerpc64-apple-darwin10";
            } else if (arch == "armv6") {
                targetTriple = "armv6-apple-darwin4.1-iphoneos";
            } else if (arch == "armv7") {
                targetTriple = "thumbv7-apple-darwin4.1-iphoneos";
            } else {
                cerr << "error: unrecognized -arch value " << arch << "\n";
                return -1;
            }
        }
#endif
        else if (strcmp(argv[i], "-target") == 0) {
            if (i+1 == argc) {
                cerr << "error: target name missing after -target\n";
                return -1;
            }
            ++i;
            targetTriple = argv[i];
            if (targetTriple.empty() || (targetTriple[0] == '-')) {
                cerr << "error: target name missing after -target\n";
                return -1;
            }
            crossCompiling = targetTriple != llvm::sys::getHostTriple();
        }
        else if (strstr(argv[i], "-L") == argv[i]) {
            string libDir = argv[i] + strlen("-L");
            if (libDir.empty()) {
                if (i+1 == argc) {
                    cerr << "error: directory missing after -L\n";
                    return -1;
                }
                ++i;
                libDir = argv[i];
                if (libDir.empty() || (libDir[0] == '-')) {
                    cerr << "error: directory missing after -L\n";
                    return -1;
                }
            }
            libSearchPath.push_back("-L" + libDir);
        }
        else if (strstr(argv[i], "-l") == argv[i]) {
            string lib = argv[i] + strlen("-l");
            if (lib.empty()) {
                if (i+1 == argc) {
                    cerr << "error: library missing after -l\n";
                    return -1;
                }
                ++i;
                lib = argv[i];
                if (lib.empty() || (lib[0] == '-')) {
                    cerr << "error: library missing after -l\n";
                    return -1;
                }
            }
            libraries.push_back("-l" + lib);
        }
        else if (strstr(argv[i], "-I") == argv[i]) {
            string path = argv[i] + strlen("-I");
            if (path.empty()) {
                if (i+1 == argc) {
                    cerr << "error: path missing after -I\n";
                    return -1;
                }
                ++i;
                path = argv[i];
                if (path.empty() || (path[0] == '-')) {
                    cerr << "error: path missing after -I\n";
                    return -1;
                }
            }
            addSearchPath(path);
        }
        else if (strstr(argv[i], "-v") == argv[i]) {
            cerr << "clay compiler ("
#ifdef HG_ID
                 << "hg id " << HG_ID << ", "
#endif
#ifdef SVN_REVISION
                 << "llvm r" << SVN_REVISION << ", "
#endif
                 << __DATE__ << ")\n";
            return 0;
        }
        else if (strcmp(argv[i], "--") == 0) {
            ++i;
            if (clayFile.empty()) {
                if (i != argc - 1) {
                    cerr << "error: clay file already specified: " << argv[i]
                         << ", unrecognized parameter: " << argv[i+1] << '\n';
                    return -1;
                }
                clayFile = argv[i];
            } else {
                if (i != argc) {
                    cerr << "error: clay file already specified: " << clayFile
                         << ", unrecognized parameter: " << argv[i] << '\n';
                    return -1;
                }
            }
        }
        else if (strcmp(argv[i], "-help") == 0
                 || strcmp(argv[i], "--help") == 0
                 || strcmp(argv[i], "/?") == 0) {
            usage(argv[0]);
            return -1;
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

    if (clayScript.empty() && clayFile.empty()) {
        cerr << "error: clay file not specified\n";
        return -1;
    }
    if (!clayScript.empty() && !clayFile.empty()) {
        cerr << "error: -e cannot be specified with input file\n";
        return -1;
    }

    if (!clayScriptImports.empty() && clayScript.empty()) {
        cerr << "error: -M specified without -e\n";
    }

    if (emitLLVM + emitAsm + emitObject > 1) {
        cerr << "error: use only one of -llvm, -asm, or -c\n";
        return -1;
    }

    if (crossCompiling && run) {
        cerr << "error: cannot use -run when cross compiling\n";
        return -1;
    }

    if (crossCompiling && !(emitLLVM || emitAsm || emitObject)
#ifdef __APPLE__
        && arch.empty()
#endif
    ) {
        cerr << "error: must use -llvm, -asm, or -c when cross compiling\n";
        return -1;
    }

    setInlineEnabled(inlineEnabled);
    setExceptionsEnabled(exceptions);

    // On Windows, instead of "*-*-win32", use the target triple:
    // "*-*-mingw32" on 32-bit
    // "*-*-mingw64" on 64-bit
    llvm::Triple llvmTriple(targetTriple);
    if (llvmTriple.getOS() == llvm::Triple::Win32) {
        if (llvmTriple.getArch() != llvm::Triple::x86_64)
            llvmTriple.setOS(llvm::Triple::MinGW32);
        else
            llvmTriple.setOS(llvm::Triple::MinGW64);
    }
    targetTriple = llvmTriple.str();

    if (!initLLVM(targetTriple)) {
        cerr << "error: unable to initialize LLVM for target " << targetTriple << "\n";
        return -1;
    };

    initTypes();

    setAbortOnError(abortOnError);

    // Try environment variables first
    char* libclayPath = getenv("CLAY_PATH");
    if( libclayPath )
    {
        // Parse the environment variable
        // Format expected is standard PATH form, i.e
        // CLAY_PATH=path1:path2:path3  (on unix)
        // CLAY_PATH=path1;path2;path3  (on windows)
        char *begin = libclayPath;
        char *end;
        do {
            end = begin;
            while (*end && (*end != ENV_SEPARATOR))
                ++end;
            addSearchPath(string(begin, end));
            begin = end + 1;
        }
        while (*end);
    }
    // Add the relative path from the executable
    llvm::sys::Path clayExe =
        llvm::sys::Path::GetMainExecutable(argv[0], (void *)&usage);
    llvm::sys::Path clayDir(llvm::sys::path::parent_path(clayExe.str()));
    llvm::sys::Path libDirDevelopment(clayDir);
    llvm::sys::Path libDirProduction1(clayDir);
    llvm::sys::Path libDirProduction2(clayDir);
    bool result;
#ifdef WIN32
    result = libDirDevelopment.appendComponent("../../../../lib-clay");
#else
    result = libDirDevelopment.appendComponent("../../../lib-clay");
#endif
    assert(result);
#ifndef WIN32
    result = libDirProduction1.appendComponent("../lib/lib-clay");
    assert(result);
#endif
    result = libDirProduction2.appendComponent("lib-clay");
    assert(result);
    addSearchPath(libDirDevelopment.str());
    addSearchPath(libDirProduction1.str());
    addSearchPath(libDirProduction2.str());
    addSearchPath(".");

    if (outputFile.empty()) {
        string clayFileBasename = basename(clayFile);

        if (emitLLVM)
            outputFile = clayFileBasename + ".ll";
        else if (emitAsm)
            outputFile = clayFileBasename + ".s";
        else if (emitObject)
            outputFile = clayFileBasename + ".o";
        else if (sharedLib)
            outputFile = DEFAULT_DLL;
        else
            outputFile = DEFAULT_EXE;
    }
    string atomicOutputFile = outputFile + ".claytmp";
    llvm::sys::Path outputFilePath(outputFile);
    llvm::sys::Path atomicOutputFilePath(atomicOutputFile);
    
    if (!run) {
        atomicOutputFilePath.eraseFromDisk();
        llvm::sys::RemoveFileOnSignal(atomicOutputFilePath);
    }

    HiResTimer loadTimer, compileTimer, llvmTimer;

    loadTimer.start();
    ModulePtr m;
    if (!clayScript.empty())
        m = loadProgramSource("-e", clayScriptImports + "main() { " + clayScript + " }");
    else
        m = loadProgram(clayFile);
    loadTimer.stop();
    compileTimer.start();
    if (sharedLib)
        codegenSharedLib(m);
    else
        codegenExe(m);
    compileTimer.stop();

    llvmTimer.start();
    unsigned optLevel = optimize ? 3 : 0;
    if (optLevel > 0)
        optimizeLLVM(llvmModule, optLevel, !(sharedLib || run));

    if (run)
        runModule(llvmModule);
    else if (emitLLVM || emitAsm || emitObject) {
        string errorInfo;
        llvm::raw_fd_ostream out(atomicOutputFile.c_str(),
                                 errorInfo,
                                 llvm::raw_fd_ostream::F_Binary);
        if (!errorInfo.empty()) {
            cerr << "error: " << errorInfo << '\n';
            return -1;
        }
        if (emitLLVM)
            generateLLVM(llvmModule, &out);
        else if (emitAsm || emitObject)
            generateAssembly(llvmModule, &out, emitObject, optLevel, sharedLib, genPIC);
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

        vector<string> arguments;
#ifdef __APPLE__
        if (!arch.empty()) {
            arguments.push_back("-arch");
            arguments.push_back(arch);
        }
        copy(
            frameworkSearchPath.begin(),
            frameworkSearchPath.end(),
            back_inserter(arguments)
        );
        copy(frameworks.begin(), frameworks.end(), back_inserter(arguments));
#endif
        copy(
            libSearchPath.begin(),
            libSearchPath.end(),
            back_inserter(arguments)
        );
        copy(libraries.begin(), libraries.end(), back_inserter(arguments));

        result = generateBinary(llvmModule, atomicOutputFile, gccPath,
                                optLevel, exceptions, sharedLib, genPIC,
                                arguments);
        if (!result)
            return -1;
    }
    llvmTimer.stop();

    if (!run) {
        string renameError;
        if (atomicOutputFilePath.renamePathOnDisk(outputFilePath, &renameError)) {
            cerr << "error: could not commit result to " << outputFile << ": " << renameError;
            return -1;
        }
    }

    if (showTiming) {
        cerr << "load time = " << loadTimer.elapsedMillis() << " ms\n";
        cerr << "compile time = " << compileTimer.elapsedMillis() << " ms\n";
        cerr << "llvm time = " << llvmTimer.elapsedMillis() << " ms\n";
    }

    return 0;
}
