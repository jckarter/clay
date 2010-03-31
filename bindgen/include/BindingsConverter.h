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
using namespace clang;


class BindingsConverter : public ASTConsumer {
public:
    BindingsConverter(ostream& out);

private:
    string allocateName(const string &base);
    string convertBuiltinType(const BuiltinType *type);
    string convertFPType(FunctionNoProtoType *type);
    string convertFPType(FunctionProtoType *type);
    string convertType(const Type *type);

public :
    virtual void HandleTopLevelDecl(DeclGroupRef DG);
    void generate();

private :
    void generateHeader();

private :
    ostream& out;
    vector<Decl*> decls;
    map<string, RecordDecl*> recordBodies;
    set<string> allocatedNames;

    map<RecordDecl*, string> anonRecordNames;
    map<string, string> recordNames;
    map<string, string> typedefNames;

    set<string> externsDeclared;
};
