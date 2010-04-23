//===--- InitHeaderSearch.cpp - Initialize header search paths ----------*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the InitHeaderSearch class.
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/Utils.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Frontend/HeaderSearchOptions.h"
#include "clang/Lex/HeaderSearch.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/System/Path.h"
#include "llvm/Config/config.h"
#ifdef _MSC_VER
  #define WIN32_LEAN_AND_MEAN 1
  #include <windows.h>
#endif
using namespace clang;
using namespace clang::frontend;

/// InitHeaderSearch - This class makes it easier to set the search paths of
///  a HeaderSearch object. InitHeaderSearch stores several search path lists
///  internally, which can be sent to a HeaderSearch object in one swoop.
class InitHeaderSearch {
  std::vector<DirectoryLookup> IncludeGroup[4];
  HeaderSearch& Headers;
  bool Verbose;
  std::string isysroot;

public:

  InitHeaderSearch(HeaderSearch &HS,
      bool verbose = false, const std::string &iSysroot = "")
    : Headers(HS), Verbose(verbose), isysroot(iSysroot) {}

  /// AddPath - Add the specified path to the specified group list.
  void AddPath(const llvm::Twine &Path, IncludeDirGroup Group,
               bool isCXXAware, bool isUserSupplied,
               bool isFramework, bool IgnoreSysRoot = false);

  /// AddGnuCPlusPlusIncludePaths - Add the necessary paths to suport a gnu
  ///  libstdc++.
  void AddGnuCPlusPlusIncludePaths(llvm::StringRef Base,
                                   llvm::StringRef ArchDir,
                                   llvm::StringRef Dir32,
                                   llvm::StringRef Dir64,
                                   const llvm::Triple &triple);

  /// AddMinGWCPlusPlusIncludePaths - Add the necessary paths to suport a MinGW
  ///  libstdc++.
  void AddMinGWCPlusPlusIncludePaths(llvm::StringRef Base,
                                     llvm::StringRef Arch,
                                     llvm::StringRef Version);

  /// AddDelimitedPaths - Add a list of paths delimited by the system PATH
  /// separator. The processing follows that of the CPATH variable for gcc.
  void AddDelimitedPaths(llvm::StringRef String);

  // AddDefaultCIncludePaths - Add paths that should always be searched.
  void AddDefaultCIncludePaths(const llvm::Triple &triple);

  // AddDefaultCPlusPlusIncludePaths -  Add paths that should be searched when
  //  compiling c++.
  void AddDefaultCPlusPlusIncludePaths(const llvm::Triple &triple);

  /// AddDefaultSystemIncludePaths - Adds the default system include paths so
  ///  that e.g. stdio.h is found.
  void AddDefaultSystemIncludePaths(const LangOptions &Lang,
                                    const llvm::Triple &triple);

  /// Realize - Merges all search path lists into one list and send it to
  /// HeaderSearch.
  void Realize();
};
