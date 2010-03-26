/* vim: set sw=4 ts=4: */
/*
 * BindingsConverter.h 
 * Implements an AST Consumer that outputs clay bindings
 *
 */

#include <iostream>
#include <vector>
#include <map>
using namespace std;

#include "clang/AST/ASTConsumer.h"
using namespace clang;


class BindingsConverter : public ASTConsumer {

    public:
        BindingsConverter(bool useCTypes=false, ostream& out=cout);

        QualType& convert(QualType& cType);
        string printType(const QualType& type);

        void printHeader();
        void printFooter();
        void printVarDecl(const VarDecl*& D);
        void printFuncDecl(const FunctionDecl*& D);
        void printEnumDecl(const EnumDecl*& D);
        void printRecordDecl(const RecordDecl*& D);

        virtual void HandleTopLevelDecl(DeclGroupRef DG);

    private:
        bool useCTypes;
        ostream& out;
};


