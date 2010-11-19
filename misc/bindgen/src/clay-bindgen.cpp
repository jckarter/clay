/* vim: set sw=4 ts=4: */
/*
 * clay-bindgen.cpp
 * This program generates clay bindings for C source files. 
 * This file wraps around the "BindingsConverter" ASTConsumer which does all
 * conversion.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdlib>
using namespace std;

#include "clang/Basic/Builtins.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Parse/ParseAST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclGroup.h"
#include "llvm/ADT/Triple.h"

#include "PPContext.h"
#include "BindingsConverter.h"
using namespace clang;

void usage(char *argv0) {
    cerr << "clay-bindgen: Generates clay bindings for C libraries\n";
    cerr << "Usage: " << argv0 << " <options> <headerfile>\n";
    cerr << "options:\n";
    cerr << "  -o <file>         - write generated bindings to file (default stdout)\n";
    cerr << "  -target <tgt>     - target platform for which to predefine macros\n";
    cerr << "  -isysroot <dir>   - use <dir> as system root for includes search\n";
#ifdef __APPLE__
    cerr << "  -arch <arch>      - predefine macros for Darwin architecture <arch>\n";
    cerr << "                      (only one -arch allowed)\n";
    cerr << "  -F<dir>           - add <dir> to framework search path\n";
#endif
    cerr << "  -I<dir>           - add <dir> to header search path\n";
    cerr << "  -x <lang>         - parse headers for language <lang> (default c).\n";
    cerr << "                      Only \"c\" and \"objective-c\" supported.\n";
    cerr << "  -match <string>   - only generate bindings for definitions from files\n";
    cerr << "                      whose full pathname contains <string>.\n";
    cerr << "                      If multiple -match parameters are given, then\n";
    cerr << "                      bindings are generated from files with pathnames\n";
    cerr << "                      containing any of the specified <string>s.\n";
    cerr << "                      By default bindings are generated for all parsed\n";
    cerr << "                      definitions.\n";
    cerr << "  -import <module>  - Add an \"import <module>.*;\" statement to the\n";
    cerr << "                      generated output\n";
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {

    if(argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "/?")) {
        usage(argv[0]);
    }

    std::string filename;
    std::string outfilename;
    std::string target = LLVM_HOSTTRIPLE;
    std::string isysroot;
    std::vector<std::string> frameworkDirs;
    std::vector<std::string> headerDirs;
    std::string language = "c";
    std::vector<std::string> matchNames;
    std::vector<std::string> importNames;
    
    int i = 1;
    for (; i < argc; ++i) {
        if (!strcmp(argv[i], "-o")) {
            ++i;
            if (i >= argc) {
                cerr << "No filename given after -o\n";
                usage(argv[0]);
            }
            outfilename = argv[i];
        } else if (!strcmp(argv[i], "-target")) {
            ++i;
            if (i >= argc) {
                cerr << "No target given after -target\n";
                usage(argv[0]);
            }
            target = argv[i];
        } else if (!strcmp(argv[i], "-isysroot")) {
            ++i;
            if (i >= argc) {
                cerr << "No path given after -isysroot\n";
                usage(argv[0]);
            }
            isysroot = argv[i];
#ifdef __APPLE__
        } else if (!strcmp(argv[i], "-arch")) {
            ++i;
            if (i >= argc) {
                cerr << "No architecture given after -arch\n";
                usage(argv[0]);
            }
            if (!strcmp(argv[i], "i386"))
                target = "i386-apple-darwin10";
            else if (!strcmp(argv[i], "x86_64"))
                target = "x86_64-apple-darwin10";
            else if (!strcmp(argv[i], "ppc"))
                target = "powerpc-apple-darwin10";
            else if (!strcmp(argv[i], "ppc64"))
                target = "powerpc64-apple-darwin10";
            else if (!strcmp(argv[i], "armv6"))
                target = "armv6-apple-darwin4.1-iphoneos";
            else if (!strcmp(argv[i], "armv7"))
                target = "thumbv7-apple-darwin4.1-iphoneos";
            else {
                cerr << "Unrecognized -arch value " << argv[i] << "\n";
                usage(argv[0]);
            }
        } else if (!strncmp(argv[i], "-F", strlen("-F"))) {
            string frameworkDir = argv[i] + 2;
            if (frameworkDir.empty()) {
                ++i;
                if (i >= argc) {
                    cerr << "No path given after -F\n";
                    usage(argv[0]);
                }
                frameworkDir = argv[i];
            }
            frameworkDirs.push_back(frameworkDir);
#endif
        } else if (!strncmp(argv[i], "-I", strlen("-I"))) {
            string headerDir = argv[i] + 2;
            if (headerDir.empty()) {
                ++i;
                if (i >= argc) {
                    cerr << "No path given after -I\n";
                    usage(argv[0]);
                }
                headerDir = argv[i];
            }
            headerDirs.push_back(headerDir);
        } else if (!strcmp(argv[i], "-x")) {
            ++i;
            if (i >= argc) {
                cerr << "No language name given after -x\n";
                usage(argv[0]);
            }
            if (strcmp(argv[i], "c") && strcmp(argv[i], "objective-c")) {
                cerr << "Unsupported language " << argv[i] << "\n";
                usage(argv[0]);
            }
            language = argv[i];
        } else if (!strcmp(argv[i], "-match")) {
            ++i;
            if (i >= argc) {
                cerr << "No string given after -match\n";
                usage(argv[0]);
            }
            matchNames.push_back(std::string(argv[i]));
        } else if (!strcmp(argv[i], "-import")) {
            ++i;
            if (i >= argc) {
                cerr << "No string given after -import\n";
                usage(argv[0]);
            }
            importNames.push_back(std::string(argv[i]));
        } else if (!strcmp(argv[i], "--")) {
            ++i;
            if (!filename.empty()) {
                if (i != argc) {
                    cerr << "Only one input file may be given\n";
                    usage(argv[0]);
                }
                break;
            } else {
                if (i != argc - 1) {
                    cerr << "Only one input file may be given\n";
                    usage(argv[0]);
                }
                filename = argv[i];
                break;
            }
        } else if (argv[i][0] != '-') {
            if (!filename.empty()) {
                cerr << "Only one input file may be given\n";
                usage(argv[0]);
            }
            filename = argv[i];
        } else {
            cerr << "Unrecognized option " << argv[i] << "\n";
            usage(argv[0]);
        }
    }

    std::ofstream *outfilestream = NULL;

    if (!outfilename.empty() && outfilename != "-") {
        outfilestream = new ofstream(outfilename.c_str());
        if (outfilestream->fail()) {
            cerr << "Unable to open " << outfilename << " for writing\n";
            return EXIT_FAILURE;
        }
    }

    std::ostream &outstream =
        outfilestream ? *outfilestream : std::cout;

    llvm::Triple triple(target);

    // Setup clang
    clang::TargetOptions targetOpts;
    targetOpts.Triple = triple.str();

    // MacOS X headers assume SSE is available
    if (triple.getOS() == llvm::Triple::Darwin && triple.getArch() == llvm::Triple::x86)
        targetOpts.Features.push_back("+sse2");

    clang::DiagnosticOptions diagOpts;
    BindingsConverter *converter = new BindingsConverter(
        outstream,
        diagOpts,
        language,
        matchNames,
        importNames
    );

    // Create a preprocessor context
    clang::LangOptions langOpts;
    langOpts.Blocks = 1; // for recent OS X/iOS headers
    langOpts.C99 = 1;
    if (language == "objective-c") {
        langOpts.ObjC1 = 1;
        langOpts.ObjC2 = 1;
    }
    PPContext context(
        targetOpts, converter, langOpts,
        isysroot, frameworkDirs, headerDirs
    ); 

    // Add input file
    const FileEntry* file = context.fm.getFile(filename);
    if (!file) {
        cerr << "Failed to open \'" << argv[1] << "\'";
        return EXIT_FAILURE;
    }
    context.sm.createMainFileID(file);

    // Initialize ASTContext
    Builtin::Context builtins(*context.target);
    builtins.InitializeBuiltins(context.pp.getIdentifierTable());

    ASTContext astContext(context.opts, context.sm, *context.target,
                          context.pp.getIdentifierTable(),
                          context.pp.getSelectorTable(),
                          builtins, 0);

    // Parse it
    converter->BeginSourceFile(langOpts, &context.pp);
    ParseAST(context.pp, converter, astContext);  // calls pp.EnterMainSourceFile() for us
    converter->EndSourceFile();

    converter->generate();

    if (outfilestream)
        outfilestream->close();

    return 0;
}

