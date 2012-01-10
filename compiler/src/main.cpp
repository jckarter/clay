#include "clay.hpp"

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
        unsigned threshold = 225;
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
    builder.populateModulePassManager(passes);
    if (optLevel > 2) {
        if (internalize) {
            vector<const char*> do_not_internalize;
            std::string msvc_ctor_name = "clayglobals_msvc_ctor_" + llvmModule->getModuleIdentifier();
            do_not_internalize.push_back("main");
            do_not_internalize.push_back(msvc_ctor_name.c_str());

            passes.add(llvm::createInternalizePass(do_not_internalize));
        }
        builder.populateLTOPassManager(passes, false, true);
    }
}

static void runModule(llvm::Module *module, vector<string> &argv, char const* const* envp)
{
    llvm::EngineBuilder eb(llvmModule);
    llvm::ExecutionEngine *engine = eb.create();
    llvm::Function *mainFunc = module->getFunction("main");

    if (!mainFunc) {
        std::cerr << "no main function to -run\n";
        delete engine;
        return;
    }

    engine->runStaticConstructorsDestructors(false);
    engine->runFunctionAsMain(mainFunc, argv, envp);
    engine->runStaticConstructorsDestructors(true);

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
                             bool emitObject,
                             unsigned optLevel,
                             bool sharedLib,
                             bool genPIC,
                             bool debug)
{
    if (optLevel < 2 || debug)
        llvm::NoFramePointerElim = true;

    llvm::FunctionPassManager fpasses(module);

    fpasses.add(new llvm::TargetData(module));
    fpasses.add(llvm::createVerifierPass());

    targetMachine->setAsmVerbosityDefault(true);

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
    bool result = targetMachine->addPassesToEmitFile(
        fpasses, fout, fileType, level);
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
                           llvm::TargetMachine *targetMachine,
                           const llvm::sys::Path &outputFilePath,
                           const llvm::sys::Path &clangPath,
                           unsigned optLevel,
                           bool /*exceptions*/,
                           bool sharedLib,
                           bool genPIC,
                           bool debug,
                           const vector<string> &arguments)
{
    llvm::sys::Path tempObj("clayobj.obj");
    string errMsg;
    if (tempObj.createTemporaryFileOnDisk(false, &errMsg)) {
        cerr << "error: " << errMsg << '\n';
        return false;
    }
    llvm::sys::RemoveFileOnSignal(tempObj);

    errMsg.clear();
    llvm::raw_fd_ostream objOut(tempObj.c_str(),
                                errMsg,
                                llvm::raw_fd_ostream::F_Binary);
    if (!errMsg.empty()) {
        cerr << "error: " << errMsg << '\n';
        return false;
    }

    generateAssembly(module, targetMachine, &objOut, true, optLevel, sharedLib, genPIC, debug);
    objOut.close();

    vector<const char *> clangArgs;
    clangArgs.push_back(clangPath.c_str());

    switch (llvmTargetData->getPointerSizeInBits()) {
    case 32 :
        clangArgs.push_back("-m32");
        break;
    case 64 :
        clangArgs.push_back("-m64");
        break;
    default :
        assert(false);
    }

    llvm::Triple triple(llvmModule->getTargetTriple());
    string linkerFlags;
    if (sharedLib) {
        clangArgs.push_back("-shared");

        if (triple.getOS() == llvm::Triple::MinGW32
            || triple.getOS() == llvm::Triple::Cygwin) {

            llvm::sys::Path defPath(outputFilePath);
            defPath.eraseSuffix();
            defPath.appendSuffix("def");

            linkerFlags = "-Wl,--output-def," + defPath.str();

            clangArgs.push_back(linkerFlags.c_str());
        }
    }
    if (debug) {
        if (triple.getOS() == llvm::Triple::Win32)
            clangArgs.push_back("-Wl,/debug");
    }
    clangArgs.push_back("-o");
    clangArgs.push_back(outputFilePath.c_str());
    clangArgs.push_back(tempObj.c_str());
    for (unsigned i = 0; i < arguments.size(); ++i)
        clangArgs.push_back(arguments[i].c_str());
    clangArgs.push_back(NULL);

    int result = llvm::sys::Program::ExecuteAndWait(clangPath, &clangArgs[0]);

    if (debug && triple.getOS() == llvm::Triple::Darwin) {
        llvm::sys::Path dsymutilPath = llvm::sys::Program::FindProgramByName("dsymutil");
        if (dsymutilPath.isValid()) {
            llvm::sys::Path outputDSYMPath = outputFilePath;
            outputDSYMPath.appendSuffix("dSYM");

            vector<const char *> dsymutilArgs;
            dsymutilArgs.push_back(dsymutilPath.c_str());
            dsymutilArgs.push_back("-o");
            dsymutilArgs.push_back(outputDSYMPath.c_str());
            dsymutilArgs.push_back(outputFilePath.c_str());
            dsymutilArgs.push_back(NULL);

            int dsymResult = llvm::sys::Program::ExecuteAndWait(dsymutilPath,
                &dsymutilArgs[0]);

            if (dsymResult != 0)
                cerr << "warning: dsymutil exited with error code " << dsymResult << "\n";
        } else
            cerr << "warning: unable to find dsymutil on the path; debug info for executable will not be generated\n";
    }

    if (tempObj.eraseFromDisk(false, &errMsg)) {
        cerr << "error: " << errMsg << '\n';
        return false;
    }

    return (result == 0);
}

static void usage(char *argv0)
{
    cerr << "usage: " << argv0 << " <options> <clay file>\n";
    cerr << "       " << argv0 << " <options> -e <clay code>\n";
    cerr << "options:\n";
    cerr << "  -o <file>             specify output file\n";
    cerr << "  -target <target>      set target platform for code generation\n";
    cerr << "  -shared               create a dynamically linkable library\n";
    cerr << "  -emit-llvm            emit llvm code\n";
    cerr << "  -S                    emit assembler code\n";
    cerr << "  -c                    emit object code\n";
    cerr << "  -DFLAG[=value]        set flag value\n"
         << "                        (queryable with Flag?() and Flag())\n";
    cerr << "  -O0 -O1 -O2 -O3       set optimization level\n";
    cerr << "  -g                    keep debug symbol information\n";
    cerr << "  -exceptions           enable exception handling\n";
    cerr << "  -no-exceptions        disable exception handling\n";
    cerr << "  -inline               inline procedures marked 'inline'\n";
    cerr << "  -no-inline            ignore 'inline' keyword\n";
    cerr << "  -import-externals     include externals from imported modules\n"
         << "                        in compilation unit\n"
         << "                        (default when building standalone or -shared)\n";
    cerr << "  -no-import-externals  don't include externals from imported modules\n"
         << "                        in compilation unit\n"
         << "                        (default when building -c or -S)\n";
    cerr << "  -pic                  generate position independent code\n";
    cerr << "  -abort                abort on error (to get stacktrace in gdb)\n";
    cerr << "  -run                  execute the program without writing to disk\n";
    cerr << "  -timing               show timing information\n";
    cerr << "  -full-match-errors    show universal patterns in match failure errors\n";
#ifdef __APPLE__
    cerr << "  -arch <arch>          build for Darwin architecture <arch>\n";
    cerr << "  -F<dir>               add <dir> to framework search path\n";
    cerr << "  -framework <name>     link with framework <name>\n";
#endif
    cerr << "  -L<dir>               add <dir> to library search path\n";
    cerr << "  -Wl,<opts>            pass flags to linker\n";
    cerr << "  -l<lib>               link with library <lib>\n";
    cerr << "  -I<path>              add <path> to clay module search path\n";
    cerr << "  -e <source>           compile and run <source> (implies -run)\n";
    cerr << "  -M<module>            \"import <module>.*;\" for -e\n";
    cerr << "  -v                    display version info\n";
}

string dirname(const string &fullname)
{
    string::size_type slash = fullname.find_last_of(PATH_SEPARATORS);
    return fullname.substr(0, slash == string::npos ? 0 : slash);
}

string basename(const string &fullname, bool chopSuffix)
{
    string::size_type to = fullname.rfind('.');
    string::size_type slash = fullname.find_last_of(PATH_SEPARATORS);
    string::size_type from = slash == string::npos ? 0 : slash+1;
    string::size_type length =
        (!chopSuffix || to == string::npos) ? string::npos : to - from;

    return fullname.substr(from, length);
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
    bool abortOnError = false;
    bool run = false;
    bool crossCompiling = false;
    bool showTiming = false;
    bool codegenExternals = false;
    bool codegenExternalsSet = false;

    unsigned optLevel = 3;
    bool optLevelSet = false;

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
    string linkerFlags;
    vector<string> libraries;
#ifdef __APPLE__
    vector<string> frameworkSearchPath;
    vector<string> frameworks;
#endif

    bool debug = false;

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
        else if (strcmp(argv[i], "-abort") == 0) {
            abortOnError = true;
        }
        else if (strcmp(argv[i], "-run") == 0) {
            run = true;
        }
        else if (strcmp(argv[i], "-timing") == 0) {
            showTiming = true;
        }
        else if (strcmp(argv[i], "-full-match-errors") == 0) {
            shouldPrintFullMatchErrors = true;
        }
        else if (strcmp(argv[i], "-e") == 0) {
            if (i+1 == argc) {
                cerr << "error: source string missing after -e\n";
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
                cerr << "error: module missing after -M\n";
                return 1;
            }
            clayScriptImports += "import " + modulespec + ".*; ";
        }
        else if (strcmp(argv[i], "-o") == 0) {
            if (i+1 == argc) {
                cerr << "error: filename missing after -o\n";
                return 1;
            }
            if (!outputFile.empty()) {
                cerr << "error: output file already specified: "
                     << outputFile
                     << ", specified again as " << argv[i] << '\n';
                return 1;
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
                    return 1;
                }
                ++i;
                frameworkDir = argv[i];
                if (frameworkDir.empty() || (frameworkDir[0] == '-')) {
                    cerr << "error: directory missing after -F\n";
                    return 1;
                }
            }
            frameworkSearchPath.push_back("-F" + frameworkDir);
        }
        else if (strcmp(argv[i], "-framework") == 0) {
            if (i+1 == argc) {
                cerr << "error: framework name missing after -framework\n";
                return 1;
            }
            ++i;
            string framework = argv[i];
            if (framework.empty() || (framework[0] == '-')) {
                cerr << "error: framework name missing after -framework\n";
                return 1;
            }
            frameworks.push_back("-framework");
            frameworks.push_back(framework);
        }
        else if (strcmp(argv[i], "-arch") == 0) {
            if (i+1 == argc) {
                cerr << "error: architecture name missing after -arch\n";
                return 1;
            }
            ++i;
            if (!arch.empty()) {
                cerr << "error: multiple -arch flags currently unsupported\n";
                return 1;
            }
            arch = argv[i];
            if (arch.empty() || (arch[0] == '-')) {
                cerr << "error: architecture name missing after -arch\n";
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
                cerr << "error: unrecognized -arch value " << arch << "\n";
                return 1;
            }
        }
#endif
        else if (strcmp(argv[i], "-target") == 0) {
            if (i+1 == argc) {
                cerr << "error: target name missing after -target\n";
                return 1;
            }
            ++i;
            targetTriple = argv[i];
            if (targetTriple.empty() || (targetTriple[0] == '-')) {
                cerr << "error: target name missing after -target\n";
                return 1;
            }
            crossCompiling = targetTriple != llvm::sys::getHostTriple();
        }
        else if (strstr(argv[i], "-Wl") == argv[i]) {
            linkerFlags += argv[i] + strlen("-Wl");
        }
        else if (strstr(argv[i], "-L") == argv[i]) {
            string libDir = argv[i] + strlen("-L");
            if (libDir.empty()) {
                if (i+1 == argc) {
                    cerr << "error: directory missing after -L\n";
                    return 1;
                }
                ++i;
                libDir = argv[i];
                if (libDir.empty() || (libDir[0] == '-')) {
                    cerr << "error: directory missing after -L\n";
                    return 1;
                }
            }
            libSearchPath.push_back("-L" + libDir);
        }
        else if (strstr(argv[i], "-l") == argv[i]) {
            string lib = argv[i] + strlen("-l");
            if (lib.empty()) {
                if (i+1 == argc) {
                    cerr << "error: library missing after -l\n";
                    return 1;
                }
                ++i;
                lib = argv[i];
                if (lib.empty() || (lib[0] == '-')) {
                    cerr << "error: library missing after -l\n";
                    return 1;
                }
            }
            libraries.push_back("-l" + lib);
        }
        else if (strstr(argv[i], "-D") == argv[i]) {
            char *namep = argv[i] + strlen("-D");
            if (namep[0] == '\0') {
                if (i+1 == argc) {
                    cerr << "error: definition missing after -D\n";
                    return 1;
                }
                ++i;
                namep = argv[i];
                if (namep[0] == '\0' || namep[0] == '-') {
                    cerr << "error: definition missing after -D\n";
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

            globalFlags[name] = value;
        }
        else if (strstr(argv[i], "-I") == argv[i]) {
            string path = argv[i] + strlen("-I");
            if (path.empty()) {
                if (i+1 == argc) {
                    cerr << "error: path missing after -I\n";
                    return 1;
                }
                ++i;
                path = argv[i];
                if (path.empty() || (path[0] == '-')) {
                    cerr << "error: path missing after -I\n";
                    return 1;
                }
            }
            addSearchPath(path);
        }
        else if (strstr(argv[i], "-v") == argv[i]
                 || strcmp(argv[i], "--version") == 0) {
            cerr << "clay compiler version " CLAY_COMPILER_VERSION
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
        else if (strcmp(argv[i], "--") == 0) {
            ++i;
            if (clayFile.empty()) {
                if (i != argc - 1) {
                    cerr << "error: clay file already specified: " << argv[i]
                         << ", unrecognized parameter: " << argv[i+1] << '\n';
                    return 1;
                }
                clayFile = argv[i];
            } else {
                if (i != argc) {
                    cerr << "error: clay file already specified: " << clayFile
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
                cerr << "error: clay file already specified: " << clayFile
                     << ", unrecognized parameter: " << argv[i] << '\n';
                return 1;
            }
            clayFile = argv[i];
        }
        else {
            cerr << "error: unrecognized option " << argv[i] << '\n';
            return 1;
        }
    }

    if (clayScript.empty() && clayFile.empty()) {
        cerr << "error: clay file not specified\n";
        return 1;
    }
    if (!clayScript.empty() && !clayFile.empty()) {
        cerr << "error: -e cannot be specified with input file\n";
        return 1;
    }

    if (!clayScriptImports.empty() && clayScript.empty()) {
        cerr << "error: -M specified without -e\n";
    }

    if (emitAsm && emitObject) {
        cerr << "error: -S or -c cannot be used together\n";
        return 1;
    }

    if (crossCompiling && run) {
        cerr << "error: cannot use -run when cross compiling\n";
        return 1;
    }

    if (crossCompiling && !(emitLLVM || emitAsm || emitObject)
#ifdef __APPLE__
        && arch.empty()
#endif
    ) {
        cerr << "error: must use -emit-llvm, -S, or -c when cross compiling\n";
        return 1;
    }

    if (!codegenExternalsSet)
        codegenExternals = !(emitLLVM || emitAsm || emitObject);

    setInlineEnabled(inlineEnabled);
    setExceptionsEnabled(exceptions);

    llvm::Triple llvmTriple(targetTriple);
    targetTriple = llvmTriple.str();

    std::string moduleName = clayScript.empty() ? clayFile : "-e";

    llvm::TargetMachine *targetMachine = initLLVM(targetTriple, moduleName, "",
        (sharedLib || genPIC), debug, optLevel > 0);
    if (targetMachine == NULL)
    {
        cerr << "error: unable to initialize LLVM for target " << targetTriple << "\n";
        return 1;
    }

    initTypes();
    initExternalTarget(targetTriple);

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
    result = libDirDevelopment.appendComponent("../../../lib-clay");
    assert(result);

    result = libDirProduction1.appendComponent("../lib/lib-clay");
    result = libDirProduction2.appendComponent("lib-clay");
    assert(result);
    addSearchPath(libDirDevelopment.str());
    addSearchPath(libDirProduction1.str());
    addSearchPath(libDirProduction2.str());
    addSearchPath(".");

    if (outputFile.empty()) {
        string clayFileBasename = basename(clayFile, true);

        if (emitLLVM && emitAsm)
            outputFile = clayFileBasename + ".ll";
        else if (emitAsm)
            outputFile = clayFileBasename + ".s";
        else if (emitObject || emitLLVM)
            outputFile = clayFileBasename + objExtensionForTarget(llvmTriple);
        else if (sharedLib)
            outputFile = clayFileBasename + sharedExtensionForTarget(llvmTriple);
        else
            outputFile = clayFileBasename + exeExtensionForTarget(llvmTriple);
    }
    llvm::sys::PathWithStatus outputFilePath(outputFile);
    const llvm::sys::FileStatus *outputFileStatus = outputFilePath.getFileStatus();
    if (outputFileStatus != NULL && outputFileStatus->isDir) {
        cerr << "error: output file '" << outputFile << "' is a directory\n";
        return 1;
    }
    llvm::sys::RemoveFileOnSignal(outputFilePath);

    HiResTimer loadTimer, compileTimer, optTimer, outputTimer;

    loadTimer.start();
    ModulePtr m;
    string clayScriptSource;
    if (!clayScript.empty()) {
        clayScriptSource = clayScriptImports + "main() {\n" + clayScript + "}";
        m = loadProgramSource("-e", clayScriptSource);
    } else
        m = loadProgram(clayFile);
    loadTimer.stop();
    compileTimer.start();
    codegenEntryPoints(m, codegenExternals);
    compileTimer.stop();

    bool internalize = true;
    if (debug || sharedLib || run || !codegenExternals)
        internalize = false;

    optTimer.start();
    if (optLevel > 0)
        optimizeLLVM(llvmModule, optLevel, internalize);
    optTimer.stop();

    if (run) {
        vector<string> argv;
        argv.push_back(clayFile);
        runModule(llvmModule, argv, envp);
    }
    else if (emitLLVM || emitAsm || emitObject) {
        string errorInfo;
        llvm::raw_fd_ostream out(outputFilePath.c_str(),
                                 errorInfo,
                                 llvm::raw_fd_ostream::F_Binary);
        if (!errorInfo.empty()) {
            cerr << "error: " << errorInfo << '\n';
            return 1;
        }
        outputTimer.start();
        if (emitLLVM)
            generateLLVM(llvmModule, emitAsm, &out);
        else if (emitAsm || emitObject)
            generateAssembly(llvmModule, targetMachine, &out, emitObject, optLevel, sharedLib, genPIC, debug);
        outputTimer.stop();
    }
    else {
        bool result;
        llvm::sys::Path clangPath;
#ifdef WIN32
        clangPath = clayDir;
        result = clangPath.appendComponent("mingw");
        assert(result);
        result = clangPath.appendComponent("bin");
        assert(result);
        result = clangPath.appendComponent("clang.exe");
        assert(result);
        if (!clangPath.exists())
            clangPath = llvm::sys::Path();
#endif
        if (!clangPath.isValid()) {
            clangPath = llvm::sys::Program::FindProgramByName("clang");
        }
        if (!clangPath.isValid()) {
            cerr << "error: unable to find clang on the path\n";
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
            libSearchPath.begin(),
            libSearchPath.end(),
            back_inserter(arguments)
        );
        copy(libraries.begin(), libraries.end(), back_inserter(arguments));

        outputTimer.start();
        result = generateBinary(llvmModule, targetMachine, outputFilePath, clangPath,
                                optLevel, exceptions, sharedLib, genPIC, debug,
                                arguments);
        outputTimer.stop();
        if (!result)
            return 1;
    }

    if (showTiming) {
        cerr << "load time = " << loadTimer.elapsedMillis() << " ms\n";
        cerr << "compile time = " << compileTimer.elapsedMillis() << " ms\n";
        cerr << "optimization time = " << optTimer.elapsedMillis() << " ms\n";
        cerr << "codegen time = " << outputTimer.elapsedMillis() << " ms\n";
    }

    return 0;
}

}

int main(int argc, char **argv, char const* const* envp) {
    return clay::parachute(clay::main2, argc, argv, envp);
}
