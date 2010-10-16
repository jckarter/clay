/* vim: set sw=4 ts=4: */
#ifndef PP_CONTEXT
#define PP_CONTEXT

#include <string>

#include "llvm/Config/config.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/System/Path.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/Version.h"
#include "clang/Basic/FileManager.h"

#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"

#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/DiagnosticOptions.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/PreprocessorOptions.h"
#include "clang/Frontend/HeaderSearchOptions.h"
#include "clang/Frontend/Utils.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace llvm;
struct PPContext {
    // Takes ownership of client.
    PPContext(clang::TargetOptions targetOpts,clang::DiagnosticClient* client, clang::LangOptions langOpts)
        : 
        diags(client),
        opts(langOpts),
        target(clang::TargetInfo::CreateTargetInfo(diags,targetOpts)),
        headers(fm),
        sm(diags),
        pp(diags, opts, *target, sm, headers)
        {
            // Configure warnings to be similar to what command-line `clang` outputs
            // (see tut03).
            // XXX: move warning initialization to libDriver
            using namespace clang;
            // diags.setDiagnosticMapping(diag::warn_pp_undef_identifier,diag::MAP_IGNORE);
            // diags.setSuppressSystemWarnings(true);
            //diags.setSuppressAllDiagnostics(true);	  

            HeaderSearchOptions hsOpts;
            llvm::sys::Path resourceDir(LLVM_LIBDIR);
            resourceDir.appendComponent("clang");
            resourceDir.appendComponent(CLANG_VERSION_STRING);

            hsOpts.ResourceDir = resourceDir.c_str();
            hsOpts.UseBuiltinIncludes = 1;
            hsOpts.UseStandardIncludes = 1;
            InitializePreprocessor(pp, PreprocessorOptions(), hsOpts, FrontendOptions());
        }

    ~PPContext()
    {
        delete target;
    }


    clang::Diagnostic diags;
    clang::LangOptions opts;
    clang::TargetInfo* target;
    clang::FileManager fm;
    clang::HeaderSearch headers;
    clang::SourceManager sm;
    clang::Preprocessor pp;
};

#endif
