/* emacs: -*- C++ -*- */
/* vim: set sw=4 ts=4: */
/*
 * BindingsConverter.h 
 * Implements an AST Consumer that outputs clay bindings
 *
 */

#include <iostream>
#include <vector>
#include <map>
#include <set>
using namespace std;

#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/DiagnosticOptions.h"
using namespace clang;


class BindingsConverter : public ASTConsumer, public TextDiagnosticPrinter {
public:
    BindingsConverter(ostream& out);

private:
    string allocateName(const string &base);
    string convertBuiltinType(const BuiltinType *type);
    string convertFPType(const FunctionNoProtoType *type);
    string convertFPType(const FunctionProtoType *type);
    string convertType(const Type *type);

public :
    virtual void HandleTopLevelDecl(DeclGroupRef DG);
    void generate();

    //virtual void HandleDiagnostic(clang::Diagnostic::Level level, const clang::DiagnosticInfo &info);

private :
    void generateDecl(Decl *decl);
    void generateHeader();

private :
    ostream& out;
    vector<Decl*> decls;
    map<string, RecordDecl*> recordBodies;
    set<string> allocatedNames;

    map<RecordDecl*, string> anonRecordNames;
    vector<RecordDecl*> pendingAnonRecords;
    map<string, string> recordNames;
    map<string, string> typedefNames;

    set<string> externsDeclared;

    bool succeeded;
};
