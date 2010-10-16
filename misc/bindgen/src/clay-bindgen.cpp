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

    llvm::Triple triple(LLVM_HOSTTRIPLE);

    // Setup clang
    clang::TargetOptions targetOpts;
    targetOpts.Triple = triple.str();

    BindingsConverter *converter = new BindingsConverter(cout);

    // Create a preprocessor context
    clang::LangOptions langOpts;
    /* XXX
    langOpts.ObjC1 = 1;
    langOpts.ObjC2 = 1;
    */
    PPContext context(targetOpts, converter, langOpts); 

    // Add input file
    const FileEntry* file = context.fm.getFile(filename);
    if (!file) {
        cerr << "Failed to open \'" << argv[1] << "\'";
        return EXIT_FAILURE;
    }
    context.sm.createMainFileID(file);

    // Initialize ASTContext
    IdentifierTable identifierTable(context.opts);
    SelectorTable selectorTable;
    Builtin::Context builtins(*context.target);

    ASTContext astContext(context.opts, context.sm, *context.target, identifierTable,
                          selectorTable, builtins, 0);

    // Parse it
    converter->BeginSourceFile(langOpts, &context.pp);
    ParseAST(context.pp, converter, astContext);  // calls pp.EnterMainSourceFile() for us
    converter->EndSourceFile();

    converter->generate();

    return 0;
}

