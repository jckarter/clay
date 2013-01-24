#include "clay.hpp"
#include "hirestimer.hpp"
#include "error.hpp"
#include "codegen.hpp"
#include "loader.hpp"
#include "invoketables.hpp"

// for _exit
#ifdef _WIN32
# include <process.h>
#else
# include <unistd.h>
#endif

namespace clay {

#ifdef WIN32
#define PATH_SEPARATORS "/\\"
#else
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
    llvm::Pass *inliningPass = 0;
    if (optLevel > 1) {
        int threshold = 225;
        if (optLevel > 2)
            threshold = 275;
        inliningPass = llvm::createFunctionInliningPass(threshold);
    } else {
        inliningPass = llvm::createAlwaysInlinerPass();
    }

    llvm::PassManagerBuilder builder;
    builder.OptLevel = optLevel;
    builder.Inliner = inliningPass;

    builder.populateFunctionPassManager(fpasses);

    // If all optimizations are disabled, just run the always-inline pass.
    if (optLevel == 0) {
        if (builder.Inliner) {
            passes.add(builder.Inliner);
            builder.Inliner = 0;
        }
    } else {
        passes.add(llvm::createTypeBasedAliasAnalysisPass());
        passes.add(llvm::createBasicAliasAnalysisPass());
        
        passes.add(llvm::createGlobalOptimizerPass());     // Optimize out global vars

        passes.add(llvm::createIPSCCPPass());              // IP SCCP
        passes.add(llvm::createDeadArgEliminationPass());  // Dead argument elimination

        passes.add(llvm::createInstructionCombiningPass());// Clean up after IPCP & DAE
        passes.add(llvm::createCFGSimplificationPass());   // Clean up after IPCP & DAE
        
        // Start of CallGraph SCC passes.
        if (builder.Inliner) {
            passes.add(builder.Inliner);
            builder.Inliner = 0;
        }
        if (optLevel > 2)
            passes.add(llvm::createArgumentPromotionPass());   // Scalarize uninlined fn args

        // Start of function pass.
        // Break up aggregate allocas, using SSAUpdater.
        passes.add(llvm::createScalarReplAggregatesPass(-1, false));
        passes.add(llvm::createEarlyCSEPass());              // Catch trivial redundancies
        
        passes.add(llvm::createJumpThreadingPass());         // Thread jumps.
        // Disable Value Propagation pass until fixed LLVM bug #12503
        // passes.add(llvm::createCorrelatedValuePropagationPass()); // Propagate conditionals
        passes.add(llvm::createCFGSimplificationPass());     // Merge & remove BBs
        passes.add(llvm::createInstructionCombiningPass());  // Combine silly seq's

        passes.add(llvm::createTailCallEliminationPass());   // Eliminate tail calls
        passes.add(llvm::createCFGSimplificationPass());     // Merge & remove BBs
        passes.add(llvm::createReassociatePass());           // Reassociate expressions
        passes.add(llvm::createLoopRotatePass());            // Rotate Loop
        passes.add(llvm::createLICMPass());                  // Hoist loop invariants
        passes.add(llvm::createLoopUnswitchPass(builder.SizeLevel || optLevel < 3));
        passes.add(llvm::createInstructionCombiningPass());
        passes.add(llvm::createIndVarSimplifyPass());        // Canonicalize indvars
        passes.add(llvm::createLoopIdiomPass());             // Recognize idioms like memset.
        passes.add(llvm::createLoopDeletionPass());          // Delete dead loops
        
        if (optLevel > 1)
            passes.add(llvm::createGVNPass());                 // Remove redundancies
        passes.add(llvm::createMemCpyOptPass());             // Remove memcpy / form memset
        passes.add(llvm::createSCCPPass());                  // Constant prop with SCCP

        // Run instcombine after redundancy elimination to exploit opportunities
        // opened up by them.
        passes.add(llvm::createInstructionCombiningPass());
        passes.add(llvm::createJumpThreadingPass());         // Thread jumps
        // Disable Value Propagation pass until fixed LLVM bug #12503
        // passes.add(llvm::createCorrelatedValuePropagationPass());
        passes.add(llvm::createDeadStoreEliminationPass());  // Delete dead stores

        if (builder.Vectorize) {
            passes.add(llvm::createBBVectorizePass());
            passes.add(llvm::createInstructionCombiningPass());
            if (optLevel > 1)
                passes.add(llvm::createGVNPass());                 // Remove redundancies
        }

        passes.add(llvm::createAggressiveDCEPass());         // Delete dead instructions
        passes.add(llvm::createCFGSimplificationPass());     // Merge & remove BBs
        passes.add(llvm::createInstructionCombiningPass());  // Clean up after everything.

        // GlobalOpt already deletes dead functions and globals, at -O3 try a
        // late pass of GlobalDCE.  It is capable of deleting dead cycles.
        if (optLevel > 2)
            passes.add(llvm::createGlobalDCEPass());         // Remove dead fns and globals.

        if (optLevel > 1)
            passes.add(llvm::createConstantMergePass());     // Merge dup global constants
    }

    if (optLevel > 2) {
        if (internalize) {
            vector<const char*> do_not_internalize;
            do_not_internalize.push_back("main");
            passes.add(llvm::createInternalizePass(do_not_internalize));
        }
        builder.populateLTOPassManager(passes, false, true);
    }
}

static bool linkLibraries(llvm::Module *module, llvm::ArrayRef<string>  libSearchPaths, llvm::ArrayRef<string>  libs, CompilerStatePtr cst)
{
    if (libs.empty())
        return true;
    llvm::Linker linker("clay", cst->llvmModule, llvm::Linker::Verbose);
    linker.addSystemPaths();
    linker.addPaths(libSearchPaths);
    for (size_t i = 0; i < libs.size(); ++i){
        string lib = libs[i];
        cst->llvmModule->addLibrary(lib);
        //as in cling/lib/Interpreter/Interpreter.cpp
        bool isNative = true;
        if (linker.LinkInLibrary(lib, isNative)) {
            // that didn't work, try bitcode:
            llvm::sys::Path FilePath(lib);
            std::string Magic;
            if (!FilePath.getMagicNumber(Magic, 64)) {
                // filename doesn't exist...
                linker.releaseModule();
                return false;
            }
            if (llvm::sys::IdentifyFileType(Magic.c_str(), 64)
                == llvm::sys::Bitcode_FileType) {
                // We are promised a bitcode file, complain if it fails
                linker.setFlags(0);
                if (linker.LinkInFile(llvm::sys::Path(lib), isNative)) {
                    linker.releaseModule();
                    return false;
                }
            } else {
                // Nothing the linker can handle
                linker.releaseModule();
                return false;
            }
        } else if (isNative) {
            // native shared library, load it!
            llvm::sys::Path SoFile = linker.FindLib(lib);
            if (SoFile.isEmpty())
            {
                llvm::errs() << "Couldn't find shared library " << lib << "\n";
                linker.releaseModule();
                return false;
            }
            std::string errMsg;
            bool hasError = llvm::sys::DynamicLibrary
                            ::LoadLibraryPermanently(SoFile.str().c_str(), &errMsg);
            if (hasError) {
                if (hasError) {
                    llvm::errs() << "Couldn't load shared library " << lib << "\n" << errMsg.c_str();
                    linker.releaseModule();
                    return false;
                }

            }
        }
    }
    linker.releaseModule();
    return true;
}

static bool runModule(llvm::Module *module,
                      vector<string> &argv,
                      char const* const* envp,
                      llvm::ArrayRef<string>  libSearchPaths,
                      llvm::ArrayRef<string>  libs,
                      CompilerStatePtr cst)
{
    if (!linkLibraries(module, libSearchPaths, libs, cst)) {
        return false;
    }
    llvm::EngineBuilder eb(cst->llvmModule);
    llvm::ExecutionEngine *engine = eb.create();
    llvm::Function *mainFunc = module->getFunction("main");
    if (!mainFunc) {
        llvm::errs() << "no main function to -run\n";
        delete engine;
        return false;
    }
    engine->runStaticConstructorsDestructors(false);
    engine->runFunctionAsMain(mainFunc, argv, envp);
    engine->runStaticConstructorsDestructors(true);

    delete engine;
    return true;
}

static void optimizeLLVM(llvm::Module *module, unsigned optLevel, bool internalize)
{
    llvm::PassManager passes;

    string moduleDataLayout = module->getDataLayout();
    llvm::DataLayout *dl = new llvm::DataLayout(moduleDataLayout);
    passes.add(dl);

    llvm::FunctionPassManager fpasses(module);

    fpasses.add(new llvm::DataLayout(*dl));

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

static void generateLLVM(llvm::Module *module, bool emitAsm, llvm::raw_ostream *out)
{
    llvm::PassManager passes;
    if (emitAsm)
        passes.add(llvm::createPrintModulePass(out));
    else
        passes.add(llvm::createBitcodeWriterPass(*out));
    passes.run(*module);
}

static void generateAssembly(llvm::Module *module,
                             llvm::TargetMachine *targetMachine,
                             llvm::raw_ostream *out,
                             bool emitObject)
{
    llvm::FunctionPassManager fpasses(module);

    fpasses.add(new llvm::DataLayout(module));
    fpasses.add(llvm::createVerifierPass());

    targetMachine->setAsmVerbosityDefault(true);

    llvm::formatted_raw_ostream fout(*out);
    llvm::TargetMachine::CodeGenFileType fileType = emitObject
        ? llvm::TargetMachine::CGFT_ObjectFile
        : llvm::TargetMachine::CGFT_AssemblyFile;

    bool result = targetMachine->addPassesToEmitFile(fpasses, fout, fileType);
    assert(!result);

    fpasses.doInitialization();
    for (llvm::Module::iterator i = module->begin(), e = module->end();
         i != e; ++i)
    {
        fpasses.run(*i);
    }
    fpasses.doFinalization();
}

static string joinCmdArgs(llvm::ArrayRef<const char*>  args) {
    string s;
    llvm::raw_string_ostream ss(s);
    for (char const * const *arg = args.begin();
         arg != args.end(); ++arg)
    {
        if (arg != args.begin()) {
            ss << " ";
        }
        if (*arg == 0 && arg + 1 == args.end()) {
            continue;
        }
        ss << *arg;
    }
    ss.flush();
    return s;
}

static bool generateBinary(llvm::Module *module,
                           llvm::TargetMachine *targetMachine,
                           llvm::Twine const &outputFilePath,
                           llvm::sys::Path const &clangPath,
                           bool /*exceptions*/,
                           bool sharedLib,
                           bool debug,
                           llvm::ArrayRef<string> arguments,
                           bool verbose,
                           CompilerStatePtr cst)
{
    int fd;
    PathString tempObj;
    if (llvm::error_code ec = llvm::sys::fs::unique_file("clayobj-%%%%%%%%.obj", fd, tempObj)) {
        llvm::errs() << "error creating temporary object file: " << ec.message() << '\n';
        return false;
    }
    llvm::sys::RemoveFileOnSignal(llvm::sys::Path(tempObj));

    {
        llvm::raw_fd_ostream objOut(fd, /*shouldClose=*/ true);

        generateAssembly(module, targetMachine, &objOut, true);
    }

    string outputFilePathStr = outputFilePath.str();

    vector<const char *> clangArgs;
    clangArgs.push_back(clangPath.c_str());

    switch (cst->llvmDataLayout->getPointerSizeInBits()) {
    case 32 :
        clangArgs.push_back("-m32");
        break;
    case 64 :
        clangArgs.push_back("-m64");
        break;
    default :
        assert(false);
    }

    llvm::Triple triple(cst->llvmModule->getTargetTriple());
    string linkerFlags;
    if (sharedLib) {
        clangArgs.push_back("-shared");

        if (triple.getOS() == llvm::Triple::MinGW32
            || triple.getOS() == llvm::Triple::Cygwin) {

            PathString defPath;
            outputFilePath.toVector(defPath);
            llvm::sys::path::replace_extension(defPath, "def");

            linkerFlags = "-Wl,--output-def," + string(defPath.begin(), defPath.end());

            clangArgs.push_back(linkerFlags.c_str());
        }
    }
    if (debug) {
        if (triple.getOS() == llvm::Triple::Win32)
            clangArgs.push_back("-Wl,/debug");
    }
    clangArgs.push_back("-o");
    clangArgs.push_back(outputFilePathStr.c_str());
    clangArgs.push_back(tempObj.c_str());
    for (unsigned i = 0; i < arguments.size(); ++i)
        clangArgs.push_back(arguments[i].c_str());
    clangArgs.push_back(NULL);

    if (verbose) {
        llvm::errs() << "executing clang to generate binary:\n";
        llvm::errs() << "    " << joinCmdArgs(clangArgs) << "\n";
    }

    int result = llvm::sys::Program::ExecuteAndWait(clangPath, &clangArgs[0]);

    if (debug && triple.getOS() == llvm::Triple::Darwin) {
        llvm::sys::Path dsymutilPath = llvm::sys::Program::FindProgramByName("dsymutil");
        if (dsymutilPath.isValid()) {
            string outputDSYMPath = outputFilePathStr;
            outputDSYMPath.append(".dSYM");

            vector<const char *> dsymutilArgs;
            dsymutilArgs.push_back(dsymutilPath.c_str());
            dsymutilArgs.push_back("-o");
            dsymutilArgs.push_back(outputDSYMPath.c_str());
            dsymutilArgs.push_back(outputFilePathStr.c_str());
            dsymutilArgs.push_back(NULL);

            if (verbose) {
                llvm::errs() << "executing dsymutil:";
                llvm::errs() << "    " << joinCmdArgs(dsymutilArgs) << "\n";
            }

            int dsymResult = llvm::sys::Program::ExecuteAndWait(dsymutilPath,
                &dsymutilArgs[0]);

            if (dsymResult != 0)
                llvm::errs() << "warning: dsymutil exited with error code " << dsymResult << "\n";
        } else
            llvm::errs() << "warning: unable to find dsymutil on the path; debug info for executable will not be generated\n";
    }

    bool dontcare;
    llvm::sys::fs::remove(llvm::StringRef(tempObj), dontcare);

    return (result == 0);
}

static void usage(char *argv0)
{
    llvm::errs() << "usage: " << argv0 << " <options> <clay file>\n";
    llvm::errs() << "       " << argv0 << " <options> -e <clay code>\n";
    llvm::errs() << "options:\n";
    llvm::errs() << "  -o <file>             specify output file\n";
    llvm::errs() << "  -target <target>      set target platform for code generation\n";
    llvm::errs() << "  -shared               create a dynamically linkable library\n";
    llvm::errs() << "  -emit-llvm            emit llvm code\n";
    llvm::errs() << "  -S                    emit assembler code\n";
    llvm::errs() << "  -c                    emit object code\n";
    llvm::errs() << "  -DFLAG[=value]        set flag value\n"
         << "                        (queryable with Flag?() and Flag())\n";
    llvm::errs() << "  -O0 -O1 -O2 -O3       set optimization level\n";
    llvm::errs() << "                        (default -O2, or -O0 with -g)\n";
    llvm::errs() << "  -g                    keep debug symbol information\n";
    llvm::errs() << "  -exceptions           enable exception handling\n";
    llvm::errs() << "  -no-exceptions        disable exception handling\n";
    llvm::errs() << "  -inline               inline procedures marked 'forceinline'\n"; 
    llvm::errs() << "                        and enable 'inline' hints (default)\n";
    llvm::errs() << "  -no-inline            ignore 'inline' and 'forceinline' keyword\n";
    llvm::errs() << "  -import-externals     include externals from imported modules\n"
         << "                        in compilation unit\n"
         << "                        (default when building standalone or -shared)\n";
    llvm::errs() << "  -no-import-externals  don't include externals from imported modules\n"
         << "                        in compilation unit\n"
         << "                        (default when building -c or -S)\n";
    llvm::errs() << "  -pic                  generate position independent code\n";
    llvm::errs() << "  -run                  execute the program without writing to disk\n";
    llvm::errs() << "  -timing               show timing information\n";
    llvm::errs() << "  -verbose              be verbose\n";
    llvm::errs() << "  -full-match-errors    show universal patterns in match failure errors\n";
    llvm::errs() << "  -log-match <module.symbol>\n"
        << "                         log overload matching behavior for calls to <symbol>\n"
        << "                         in module <module>\n";
#ifdef __APPLE__
    llvm::errs() << "  -arch <arch>          build for Darwin architecture <arch>\n";
    llvm::errs() << "  -F<dir>               add <dir> to framework search path\n";
    llvm::errs() << "  -framework <name>     link with framework <name>\n";
#endif
    llvm::errs() << "  -L<dir>               add <dir> to library search path\n";
    llvm::errs() << "  -Wl,<opts>            pass flags to linker\n";
    llvm::errs() << "  -l<lib>               link with library <lib>\n";
    llvm::errs() << "  -I<path>              add <path> to clay module search path\n";
    llvm::errs() << "  -deps                 keep track of the dependencies of the currently\n";
    llvm::errs() << "                        compiling file and write them to the file\n";
    llvm::errs() << "                        specified by -o-deps\n";
    llvm::errs() << "  -no-deps              don't generate dependencies file\n";
    llvm::errs() << "  -o-deps <file>        write the dependencies to this file\n";
    llvm::errs() << "                        (defaults to <compilation output file>.d)\n";
    llvm::errs() << "  -e <source>           compile and run <source> (implies -run)\n";
    llvm::errs() << "  -M<module>            \"import <module>.*;\" for -e\n";
    llvm::errs() << "  -version              display version info\n";

    llvm::errs() << "  -final-overloads      enable final overloads (temporary option)\n";
}

static string sharedExtensionForTarget(llvm::Triple const &triple) {
    if (triple.getOS() == llvm::Triple::Win32
        || triple.getOS() == llvm::Triple::MinGW32
        || triple.getOS() == llvm::Triple::Cygwin) {
        return ".dll";
    } else if (triple.getOS() == llvm::Triple::Darwin) {
        return ".dylib";
    } else {
        return ".so";
    }
}

static string objExtensionForTarget(llvm::Triple const &triple) {
    if (triple.getOS() == llvm::Triple::Win32) {
        return ".obj";
    } else {
        return ".o";
    }
}

static string exeExtensionForTarget(llvm::Triple const &triple) {
    if (triple.getOS() == llvm::Triple::Win32
        || triple.getOS() == llvm::Triple::MinGW32
        || triple.getOS() == llvm::Triple::Cygwin) {
        return ".exe";
    } else {
        return "";
    }
}

static void printVersion() {
    llvm::errs() << "clay compiler version " CLAY_COMPILER_VERSION
            ", language version " CLAY_LANGUAGE_VERSION " ("
#ifdef GIT_ID
         << "git id " << GIT_ID << ", "
#endif
#ifdef HG_ID
         << "hg id " << HG_ID << ", "
#endif
#ifdef SVN_REVISION
         << "llvm r" << SVN_REVISION << ", "
#endif
         << __DATE__ << ")\n";
}

int main2(int argc, char **argv, char const* const* envp) {
    if (argc == 1) {
        usage(argv[0]);
        return 2;
    }

    bool emitLLVM = false;
    bool emitAsm = false;
    bool emitObject = false;
    bool sharedLib = false;
    bool genPIC = false;
    bool inlineEnabled = true;
    bool exceptions = true;
    bool run = false;
    bool repl = false;
    bool verbose = false;
    bool crossCompiling = false;
    bool showTiming = false;
    bool codegenExternals = false;
    bool codegenExternalsSet = false;

    bool generateDeps = false;

    unsigned optLevel = 2;
    bool optLevelSet = false;

    bool finalOverloadsEnabled = false;

#ifdef __APPLE__
    genPIC = true;

    string arch;
#endif

    string clayFile;
    string outputFile;
    string targetTriple = llvm::sys::getDefaultTargetTriple();

    string clayScriptImports;
    string clayScript;

    vector<string> libSearchPathArgs;
    vector<string> libSearchPath;
    string linkerFlags;
    vector<string> librariesArgs;
    vector<string> libraries;
    vector<PathString> searchPath;

    string dependenciesOutputFile;
#ifdef __APPLE__
    vector<string> frameworkSearchPath;
    vector<string> frameworks;
#endif

    bool debug = false;

    CompilerStatePtr cst = new CompilerState();

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-shared") == 0))
        {
            sharedLib = true;
        }
        else if (strcmp(argv[i], "-emit-llvm") == 0) {
            emitLLVM = true;
        }
        else if (strcmp(argv[i], "-S") == 0) {
            emitAsm = true;
        }
        else if (strcmp(argv[i], "-c") == 0) {
            emitObject = true;
        }
        else if (strcmp(argv[i], "-g") == 0) {
            debug = true;
            if (!optLevelSet)
                optLevel = 0;
        }
        else if (strcmp(argv[i], "-O0") == 0) {
            optLevel = 0;
            optLevelSet = true;
        }
        else if (strcmp(argv[i], "-O1") == 0) {
            optLevel = 1;
            optLevelSet = true;
        }
        else if (strcmp(argv[i], "-O2") == 0) {
            optLevel = 2;
            optLevelSet = true;
        }
        else if (strcmp(argv[i], "-O3") == 0) {
            optLevel = 3;
            optLevelSet = true;
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
        else if (strcmp(argv[i], "-verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        }
        else if (strcmp(argv[i], "-run") == 0) {
            run = true;
        }
        else if (strcmp(argv[i], "-repl") == 0) {
            repl = true;
        }
        else if (strcmp(argv[i], "-timing") == 0) {
            showTiming = true;
        }
        else if (strcmp(argv[i], "-full-match-errors") == 0) {
            shouldPrintFullMatchErrors = true;
        }
        else if (strcmp(argv[i], "-log-match") == 0) {
            if (i+1 == argc) {
                llvm::errs() << "error: symbol name missing after -log-match\n";
                return 1;
            }
            ++i;
            char const *dot = strrchr(argv[i], '.');
            if (dot == NULL) {
                logMatchSymbols.insert(make_pair(string("*"), argv[i]));
            } else {
                logMatchSymbols.insert(make_pair(string((char const*)argv[i], dot), string(dot+1)));
            }
        }
        else if (strcmp(argv[i], "-e") == 0) {
            if (i+1 == argc) {
                llvm::errs() << "error: source string missing after -e\n";
                return 1;
            }
            ++i;
            run = true;
            clayScript += argv[i];
            clayScript += "\n";
        }
        else if (strncmp(argv[i], "-M", 2) == 0) {
            string modulespec = argv[i]+2;
            if (modulespec.empty()) {
                llvm::errs() << "error: module missing after -M\n";
                return 1;
            }
            clayScriptImports += "import " + modulespec + ".*; ";
        }
        else if (strcmp(argv[i], "-o") == 0) {
            ++i;
            if (i == argc) {
                llvm::errs() << "error: filename missing after -o\n";
                return 1;
            }
            if (!outputFile.empty()) {
                llvm::errs() << "error: output file already specified: "
                     << outputFile
                     << ", specified again as " << argv[i] << '\n';
                return 1;
            }
            outputFile = argv[i];
        }
#ifdef __APPLE__
        else if (strstr(argv[i], "-F") == argv[i]) {
            string frameworkDir = argv[i] + strlen("-F");
            if (frameworkDir.empty()) {
                if (i+1 == argc) {
                    llvm::errs() << "error: directory missing after -F\n";
                    return 1;
                }
                ++i;
                frameworkDir = argv[i];
                if (frameworkDir.empty() || (frameworkDir[0] == '-')) {
                    llvm::errs() << "error: directory missing after -F\n";
                    return 1;
                }
            }
            frameworkSearchPath.push_back("-F" + frameworkDir);
        }
        else if (strcmp(argv[i], "-framework") == 0) {
            if (i+1 == argc) {
                llvm::errs() << "error: framework name missing after -framework\n";
                return 1;
            }
            ++i;
            string framework = argv[i];
            if (framework.empty() || (framework[0] == '-')) {
                llvm::errs() << "error: framework name missing after -framework\n";
                return 1;
            }
            frameworks.push_back("-framework");
            frameworks.push_back(framework);
        }
        else if (strcmp(argv[i], "-arch") == 0) {
            if (i+1 == argc) {
                llvm::errs() << "error: architecture name missing after -arch\n";
                return 1;
            }
            ++i;
            if (!arch.empty()) {
                llvm::errs() << "error: multiple -arch flags currently unsupported\n";
                return 1;
            }
            arch = argv[i];
            if (arch.empty() || (arch[0] == '-')) {
                llvm::errs() << "error: architecture name missing after -arch\n";
                return 1;
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
                llvm::errs() << "error: unrecognized -arch value " << arch << "\n";
                return 1;
            }
        }
#endif
        else if (strcmp(argv[i], "-target") == 0) {
            if (i+1 == argc) {
                llvm::errs() << "error: target name missing after -target\n";
                return 1;
            }
            ++i;
            targetTriple = argv[i];
            if (targetTriple.empty() || (targetTriple[0] == '-')) {
                llvm::errs() << "error: target name missing after -target\n";
                return 1;
            }
            crossCompiling = targetTriple != llvm::sys::getDefaultTargetTriple();
        }
        else if (strstr(argv[i], "-Wl") == argv[i]) {
            linkerFlags += argv[i] + strlen("-Wl");
        }
        else if (strstr(argv[i], "-L") == argv[i]) {
            string libDir = argv[i] + strlen("-L");
            if (libDir.empty()) {
                if (i+1 == argc) {
                    llvm::errs() << "error: directory missing after -L\n";
                    return 1;
                }
                ++i;
                libDir = argv[i];
                if (libDir.empty() || (libDir[0] == '-')) {
                    llvm::errs() << "error: directory missing after -L\n";
                    return 1;
                }
            }
            libSearchPath.push_back(libDir);
            libSearchPathArgs.push_back("-L" + libDir);
        }
        else if (strstr(argv[i], "-l") == argv[i]) {
            string lib = argv[i] + strlen("-l");
            if (lib.empty()) {
                if (i+1 == argc) {
                    llvm::errs() << "error: library missing after -l\n";
                    return 1;
                }
                ++i;
                lib = argv[i];
                if (lib.empty() || (lib[0] == '-')) {
                    llvm::errs() << "error: library missing after -l\n";
                    return 1;
                }
            }
            libraries.push_back(lib);
            librariesArgs.push_back("-l" + lib);
        }
        else if (strstr(argv[i], "-D") == argv[i]) {
            char *namep = argv[i] + strlen("-D");
            if (namep[0] == '\0') {
                if (i+1 == argc) {
                    llvm::errs() << "error: definition missing after -D\n";
                    return 1;
                }
                ++i;
                namep = argv[i];
                if (namep[0] == '\0' || namep[0] == '-') {
                    llvm::errs() << "error: definition missing after -D\n";
                    return 1;
                }
            }
            char *equalSignp = strchr(namep, '=');
            string name = equalSignp == NULL
                ? string(namep)
                : string(namep, equalSignp);
            string value = equalSignp == NULL
                ? string()
                : string(equalSignp + 1);

            cst->globalFlags[name] = value;
        }
        else if (strstr(argv[i], "-I") == argv[i]) {
            string path = argv[i] + strlen("-I");
            if (path.empty()) {
                if (i+1 == argc) {
                    llvm::errs() << "error: path missing after -I\n";
                    return 1;
                }
                ++i;
                path = argv[i];
                if (path.empty() || (path[0] == '-')) {
                    llvm::errs() << "error: path missing after -I\n";
                    return 1;
                }
            }
            searchPath.push_back(PathString(path));
        }
        else if (strstr(argv[i], "-version") == argv[i]
                 || strcmp(argv[i], "--version") == 0) {
            printVersion();
            return 0;
        }
        else if (strcmp(argv[i], "-import-externals") == 0) {
            codegenExternals = true;
            codegenExternalsSet = true;
        }
        else if (strcmp(argv[i], "-no-import-externals") == 0) {
            codegenExternals = false;
            codegenExternalsSet = true;
        }
        else if (strcmp(argv[i], "-deps") == 0) {
            generateDeps = true;
        }
        else if (strcmp(argv[i], "-no-deps") == 0) {
            generateDeps = false;
        }
        else if (strcmp(argv[i], "-o-deps") == 0) {
            ++i;
            if (i == argc) {
                llvm::errs() << "error: filename missing after -o-deps\n";
                return 1;
            }
            if (!dependenciesOutputFile.empty()) {
                llvm::errs() << "error: dependencies output file already specified: "
                     << dependenciesOutputFile
                     << ", specified again as " << argv[i] << '\n';
                return 1;
            }
            dependenciesOutputFile = argv[i];
        }
        else if (strcmp(argv[i], "--") == 0) {
            ++i;
            if (clayFile.empty()) {
                if (i != argc - 1) {
                    llvm::errs() << "error: clay file already specified: " << argv[i]
                         << ", unrecognized parameter: " << argv[i+1] << '\n';
                    return 1;
                }
                clayFile = argv[i];
            } else {
                if (i != argc) {
                    llvm::errs() << "error: clay file already specified: " << clayFile
                         << ", unrecognized parameter: " << argv[i] << '\n';
                    return 1;
                }
            }
        }
        else if (strcmp(argv[i], "-help") == 0
                 || strcmp(argv[i], "--help") == 0
                 || strcmp(argv[i], "/?") == 0) {
            usage(argv[0]);
            return 2;
        }       
        else if (strstr(argv[i], "-") != argv[i]) {
            if (!clayFile.empty()) {
                llvm::errs() << "error: clay file already specified: " << clayFile
                     << ", unrecognized parameter: " << argv[i] << '\n';
                return 1;
            }
            clayFile = argv[i];
        }
        else if (strcmp(argv[i], "-final-overloads") == 0) {
            finalOverloadsEnabled = true;
        }
        else {
            llvm::errs() << "error: unrecognized option " << argv[i] << '\n';
            return 1;
        }
    }

    if (verbose) {
        printVersion();
    }

    if (repl && clayScript.empty() && clayFile.empty()) {
        clayScript = "/*empty module if file not specified*/";
    }
    else {
        if (clayScript.empty() && clayFile.empty()) {
            llvm::errs() << "error: clay file not specified\n";
            return 1;
        }
        if (!clayScript.empty() && !clayFile.empty()) {
            llvm::errs() << "error: -e cannot be specified with input file\n";
            return 1;
        }
    }

    if (!clayScriptImports.empty() && clayScript.empty()) {
        llvm::errs() << "error: -M specified without -e\n";
    }

    if (emitAsm && emitObject) {
        llvm::errs() << "error: -S or -c cannot be used together\n";
        return 1;
    }

    if (crossCompiling && run) {
        llvm::errs() << "error: cannot use -run when cross compiling\n";
        return 1;
    }

    if (crossCompiling && !(emitLLVM || emitAsm || emitObject)
#ifdef __APPLE__
        && arch.empty()
#endif
    ) {
        llvm::errs() << "error: must use -emit-llvm, -S, or -c when cross compiling\n";
        return 1;
    }

    if (!codegenExternalsSet)
        codegenExternals = !(emitLLVM || emitAsm || emitObject);

    if ((emitLLVM || emitAsm || emitObject) && run)
        run = false;

    setInlineEnabled(inlineEnabled, cst);
    setExceptionsEnabled(exceptions, cst);
    
    setFinalOverloadsEnabled(finalOverloadsEnabled, cst);
    
    llvm::Triple llvmTriple(targetTriple);
    targetTriple = llvmTriple.str();

    std::string moduleName = clayScript.empty() ? clayFile : "-e";

    llvm::TargetMachine *targetMachine = initLLVM(targetTriple, moduleName, "",
        (sharedLib || genPIC), debug, optLevel, cst);
    if (targetMachine == NULL)
    {
        llvm::errs() << "error: unable to initialize LLVM for target " << targetTriple << "\n";
        return 1;
    }

    initTypes(cst);
    initExternalTarget(targetTriple, cst);

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
            searchPath.push_back(llvm::StringRef(begin, (size_t)(end-begin)));
            begin = end + 1;
        }
        while (*end);
    }
    // Add the relative path from the executable
    PathString clayExe(llvm::sys::Path::GetMainExecutable(argv[0], (void *)(uintptr_t)&usage).c_str());
    llvm::StringRef clayDir = llvm::sys::path::parent_path(clayExe);

    PathString libDirDevelopment(clayDir);
    PathString libDirProduction1(clayDir);
    PathString libDirProduction2(clayDir);

    llvm::sys::path::append(libDirDevelopment, "../../lib-clay");
    llvm::sys::path::append(libDirProduction1, "../lib/lib-clay");
    llvm::sys::path::append(libDirProduction2, "lib-clay");

    searchPath.push_back(libDirDevelopment);
    searchPath.push_back(libDirProduction1);
    searchPath.push_back(libDirProduction2);
    searchPath.push_back(PathString("."));

    if (verbose) {
        llvm::errs() << "using search path:\n";

        for (std::vector<PathString>::const_iterator it = searchPath.begin();
                it != searchPath.end(); ++it)
        {
            llvm::errs() << "    " << *it << "\n";
        }
    }

    setSearchPath(searchPath, cst);


    if (outputFile.empty()) {
        llvm::StringRef clayFileBasename = llvm::sys::path::stem(clayFile);
        outputFile = string(clayFileBasename.begin(), clayFileBasename.end());

        if (emitLLVM && emitAsm)
            outputFile += ".ll";
        else if (emitAsm)
            outputFile += ".s";
        else if (emitObject || emitLLVM)
            outputFile += objExtensionForTarget(llvmTriple);
        else if (sharedLib)
            outputFile += sharedExtensionForTarget(llvmTriple);
        else
            outputFile += exeExtensionForTarget(llvmTriple);
    }
    if (!run) {
        bool isDir;
        if (!llvm::sys::fs::is_directory(outputFile, isDir) && isDir) {
            llvm::errs() << "error: output file '" << outputFile << "' is a directory\n";
            return 1;
        }
        llvm::sys::RemoveFileOnSignal(llvm::sys::Path(outputFile));
    }

    if (generateDeps) {
        if (run) {
            llvm::errs() << "error: '-deps' can not be used together with '-e' or '-run'\n";
            return 1;
        }
        if (dependenciesOutputFile.empty()) {
            dependenciesOutputFile = outputFile;
            dependenciesOutputFile += ".d";
        }
    }

    if (generateDeps) {
        bool isDir;
        if (!llvm::sys::fs::is_directory(dependenciesOutputFile, isDir) && isDir) {
            llvm::errs() << "error: dependencies output file '" << dependenciesOutputFile << "' is a directory\n";
            return 1;
        }
        llvm::sys::RemoveFileOnSignal(llvm::sys::Path(dependenciesOutputFile));
    }

    HiResTimer loadTimer, compileTimer, optTimer, outputTimer;


    loadTimer.start();
    try {
        initLoader(cst);

        ModulePtr m;
        string clayScriptSource;
        vector<string> sourceFiles;
        if (!clayScript.empty()) {
            clayScriptSource = clayScriptImports + "main() {\n" + clayScript + "}";
            m = loadProgramSource("-e", clayScriptSource, verbose, repl, cst);
        } else if (generateDeps)
            m = loadProgram(clayFile, &sourceFiles, verbose, repl, cst);
        else
            m = loadProgram(clayFile, NULL, verbose, repl, cst);

        loadTimer.stop();
        compileTimer.start();
        codegenEntryPoints(m, codegenExternals);
        compileTimer.stop();

        if (generateDeps) {
            string errorInfo;

            if (verbose) {
                llvm::errs() << "generating dependencies into " << dependenciesOutputFile << "\n";
            }

            llvm::raw_fd_ostream dependenciesOut(dependenciesOutputFile.c_str(),
                                                 errorInfo,
                                                 llvm::raw_fd_ostream::F_Binary);
            if (!errorInfo.empty()) {
                llvm::errs() << "error: " << errorInfo << '\n';
                return 1;
            }
            dependenciesOut << outputFile << ": \\\n";
            for (size_t i = 0; i < sourceFiles.size(); ++i) {
                dependenciesOut << "  " << sourceFiles[i];
                if (i < sourceFiles.size() - 1)
                    dependenciesOut << " \\\n";
            }
        }

        bool internalize = true;
        if (debug || sharedLib || run || !codegenExternals)
            internalize = false;

        optTimer.start();

        if (!repl)
        {
            if (optLevel > 0)
                optimizeLLVM(cst->llvmModule, optLevel, internalize);
        }
        optTimer.stop();

        if (run) {
            vector<string> argv;
            argv.push_back(clayFile);
            runModule(cst->llvmModule, argv, envp, libSearchPath, libraries, cst);
        }
        else if (repl) {
            linkLibraries(cst->llvmModule, libSearchPath, libraries, cst);
            runInteractive(m);
        }
        else if (emitLLVM || emitAsm || emitObject) {
            string errorInfo;
            llvm::raw_fd_ostream out(outputFile.c_str(),
                                     errorInfo,
                                     llvm::raw_fd_ostream::F_Binary);
            if (!errorInfo.empty()) {
                llvm::errs() << "error: " << errorInfo << '\n';
                return 1;
            }
            outputTimer.start();
            if (emitLLVM)
                generateLLVM(cst->llvmModule, emitAsm, &out);
            else if (emitAsm || emitObject)
                generateAssembly(cst->llvmModule, targetMachine, &out, emitObject);
            outputTimer.stop();
        }
        else {
            bool result;
            llvm::sys::Path clangPath = llvm::sys::Program::FindProgramByName("clang");
            if (!clangPath.isValid()) {
                llvm::errs() << "error: unable to find clang on the path\n";
                return 1;
            }

            vector<string> arguments;
#ifdef __APPLE__
            if (!arch.empty()) {
                arguments.push_back("-arch");
                arguments.push_back(arch);
            }
#endif
            if (!linkerFlags.empty())
                arguments.push_back("-Wl" + linkerFlags);
#ifdef __APPLE__
            copy(
                    frameworkSearchPath.begin(),
                    frameworkSearchPath.end(),
                    back_inserter(arguments)
                    );
            copy(frameworks.begin(), frameworks.end(), back_inserter(arguments));
#endif
            copy(
                    libSearchPathArgs.begin(),
                    libSearchPathArgs.end(),
                    back_inserter(arguments)
                    );
            copy(librariesArgs.begin(), librariesArgs.end(), back_inserter(arguments));

            outputTimer.start();
            result = generateBinary(cst->llvmModule, targetMachine, 
                                    outputFile, clangPath,
                                    exceptions, sharedLib, debug, 
                                    arguments, verbose, cst);
            outputTimer.stop();
            if (!result)
                return 1;
        }
    } catch (const CompilerError&) {
        return 1;
    }
    if (showTiming) {
        llvm::errs() << "load time = " << (size_t)loadTimer.elapsedMillis() << " ms\n";
        llvm::errs() << "compile time = " << (size_t)compileTimer.elapsedMillis() << " ms\n";
        llvm::errs() << "optimization time = " << (size_t)optTimer.elapsedMillis() << " ms\n";
        llvm::errs() << "codegen time = " << (size_t)outputTimer.elapsedMillis() << " ms\n";
        llvm::errs().flush();
    }

    _exit(0);
}

}

int main(int argc, char **argv, char const* const* envp) {
    return clay::parachute(clay::main2, argc, argv, envp);
}
