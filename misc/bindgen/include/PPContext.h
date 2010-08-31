/* vim: set sw=4 ts=4: */
#ifndef PP_CONTEXT
#define PP_CONTEXT

#include <string>

#include "llvm/Config/config.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/FileManager.h"

#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"

#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/DiagnosticOptions.h"
#include "clang/Basic/TargetOptions.h"

using namespace clang;
using namespace llvm;
struct PPContext {
    // Takes ownership of client.
    PPContext(clang::TargetOptions targetOpts,clang::DiagnosticClient* client = 0)
        : 
        diags(client == 0?new clang::TextDiagnosticPrinter(llvm::errs(),diagOptions):client),
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
            diags.setSuppressAllDiagnostics(true);	  
        }

    ~PPContext()
    {
        delete target;
    }


    clang::DiagnosticOptions diagOptions; 
    clang::Diagnostic diags;
    clang::LangOptions opts;
    clang::TargetInfo* target;
    clang::HeaderSearch headers;
    clang::SourceManager sm;
    clang::FileManager fm;
    clang::Preprocessor pp;
};

#endif
