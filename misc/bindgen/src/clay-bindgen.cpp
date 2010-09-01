/* vim: set sw=4 ts=4: */
/*
 * clay-bindgen.cpp
 * This program generates clay bindings for C source files. 
 * This file wraps around the "BindingsConverter" ASTConsumer which does all
 * conversion.
 */

#include <iostream>
#include <vector>
using namespace std;

#include "clang/Basic/Builtins.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Parse/ParseAST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclGroup.h"
#include "llvm/ADT/Triple.h"

#include "InitHeaderSearch.h"
#include "PPContext.h"
#include "BindingsConverter.h"
using namespace clang;

int main(int argc, char* argv[]) {
    string filename;

    if(argc == 2) {
        filename = argv[1];
    }
    else {
        cerr << "clay-bindgen: Generates clay bindings for C libraries" << endl;
        cerr << "Usage: " << argv[0] << " <c-file-to-be-parsed>" <<  endl;
        return EXIT_FAILURE;
    }

    // Setup clang
    clang::TargetOptions targetOpts;
    targetOpts.Triple = LLVM_HOSTTRIPLE;

    // Create a preprocessor context
    PPContext context(targetOpts);  

    // Add header search directories
    InitHeaderSearch init(context.headers);
    llvm::Triple triple;
    init.AddDefaultSystemIncludePaths(context.opts,triple);
    init.Realize();

    // Add input file
    const FileEntry* file = context.fm.getFile(filename);
    if (!file) {
        cerr << "Failed to open \'" << argv[1] << "\'";
        return EXIT_FAILURE;
    }
    context.sm.createMainFileID(file);

    BindingsConverter converter(cout);

    // Initialase ASTContext
    IdentifierTable identifierTable(context.opts);
    SelectorTable selectorTable;
    Builtin::Context builtins(*context.target);

    ASTContext astContext(context.opts, context.sm, *context.target, identifierTable,
                          selectorTable, builtins, 0);

    // Parse it
    ParseAST(context.pp, &converter, astContext);  // calls pp.EnterMainSourceFile() for us
    converter.generate();

    return 0;
}

