/* vim: set sw=4 ts=4: */
/*
 * BindingConverter.cpp
 * Implements an AST Consumer that outputs clay bindings
 *
 */

#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

#include "llvm/Config/config.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclGroup.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/Triple.h"

#include "BindingsConverter.h"

using namespace clang;
using namespace llvm;

BindingsConverter::BindingsConverter(
    ostream& out,
    clang::DiagnosticOptions const &diagOpts,
    std::string const &lang,
    std::vector<string> const &match,
    std::vector<string> const &import
)   :   TextDiagnosticPrinter(llvm::errs(), diagOpts), 
        out(out), succeeded(true),
        language(lang), matchNames(match), importNames(import),
        ast(NULL)
{ }

string BindingsConverter::allocateName(const string &base)
{
    if (!allocatedNames.count(base)) {
        allocatedNames.insert(base);
        return base;
    }
    unsigned i = 2;
    string s;
    while (true) {
        ostringstream out;
        out << base << i;
        s = out.str();
        if (!allocatedNames.count(s))
            break;
        ++i;
    }
    allocatedNames.insert(s);
    return s;
}

string BindingsConverter::convertBuiltinType(const BuiltinType *type) {
    switch (type->getKind()) {
    case BuiltinType::Void : return "Void";

    case BuiltinType::Bool : return "Bool";

    case BuiltinType::Char_U : return "CUChar";
    case BuiltinType::UChar : return "CUChar";
    case BuiltinType::UShort : return "UShort";
    case BuiltinType::UInt : return "UInt";
    case BuiltinType::ULong : return "CULong";
    case BuiltinType::ULongLong : return "UInt64";

    case BuiltinType::Char_S : return "CChar";
    case BuiltinType::SChar : return "CChar";
    case BuiltinType::Short : return "Short";
    case BuiltinType::Int : return "Int";
    case BuiltinType::Long : return "CLong";
    case BuiltinType::LongLong : return "Int64";

    case BuiltinType::Float : return "Float";
    case BuiltinType::Double : return "Double";

    default : {
        ostringstream out;
        out << type->getKind();
        return "UnsupportedCBuiltinType" + out.str();
    }
    }
}

static const char *getCodePointerConstructor(const FunctionType *ft)
{
    switch (ft->getCallConv()) {
    case CC_X86StdCall : return "StdCallCodePointer";
    case CC_X86FastCall : return "FastCallCodePointer";
    default : return "CCodePointer";
    }
}

string BindingsConverter::convertFPType(const FunctionNoProtoType *type)
{
    const Type *rt = type->getResultType().getTypePtr();
    ostringstream sout;
    sout << getCodePointerConstructor(type);
    sout << "[(),";
    string s = convertType(rt);
    if (s == "Void")
        s = "";
    sout << "(" << s << ")]";
    return sout.str();
}

string BindingsConverter::convertFPType(const FunctionProtoType *type)
{
    ostringstream sout;
    sout << getCodePointerConstructor(type);
    sout << "[(";
    FunctionProtoType::arg_type_iterator i, e;
    bool first = true;
    for (i = type->arg_type_begin(), e = type->arg_type_end(); i != e; ++i) {
        if (!first)
            sout << ",";
        else
            first = false;
        const Type *argType = i->getTypePtr();
        sout << convertType(argType);
    }
    sout << "),";
    const Type *rt = type->getResultType().getTypePtr();
    string s = convertType(rt);
    if (s == "Void")
        s = "";
    sout << "(" << s << ")]";
    return sout.str();
}

string BindingsConverter::convertObjcType(const Type *type) {
    if (type->isObjCIdType() || type->isObjCQualifiedIdType())
        return "Id";
    else if (type->isObjCClassType() || type->isObjCQualifiedClassType())
        return "Id"; // XXX Class
    else if (type->isObjCSelType())
        return "SelectorHandle"; // XXX Selector
    else if (type->isObjCObjectPointerType()) {
        const ObjCObjectPointerType *ptype = (const ObjCObjectPointerType*)type;
        const ObjCObjectType *otype = ptype->getObjectType();
        string name = otype->getInterface()->getNameAsString();
        if (name == "Protocol")
            return "Id"; // XXX Protocols
        else
            return name;
    } else {
        cerr << "warning: unknown Objective-C type " << type->getCanonicalTypeInternal().getAsString() << "\n";
        return "UnknownType";
    }
}

string BindingsConverter::convertType(const Type *type)
{
    if (language == "objective-c" && QualType(type, 0).getAsString() == "BOOL")
        return "Bool";

    const Type *ctype = type->getCanonicalTypeUnqualified().getTypePtr();
    if (ctype->getTypeClass() == Type::Builtin)
        return convertBuiltinType((const BuiltinType *)ctype);

    if (ctype->isObjCObjectPointerType() || ctype->isObjCSelType()) {
        string claytype = convertObjcType(ctype);
        return claytype;
    }

    switch (type->getTypeClass()) {
    case Type::Typedef : {
        const TypedefType *t = (const TypedefType *)type;
        TypedefDecl *decl = t->getDecl();
        string name = decl->getName().str();
        const string &outName = typedefNames[name];
        assert(!outName.empty());
        return outName;
    }
    case Type::Record : {
        const RecordType *t = (const RecordType *)type;
        RecordDecl *decl = t->getDecl();
        if (decl->isUnion())
            return "AUnionType";
        string name = decl->getName().str();
        if (name.empty()) {
            if (!anonRecordNames.count(decl)) {
                pendingAnonRecords.push_back(decl);
                string outName = allocateName("UnnamedRecord");
                anonRecordNames[decl] = outName;
            }
            return anonRecordNames[decl];
        }
        if (recordBodies.count(name)) {
            return recordNames[name];
        }
        return "Opaque";
    }
    case Type::Enum :
        return "Int";
    case Type::Pointer : {
        const PointerType *t = (const PointerType *)type;
        const Type *pt = t->getPointeeType().getTypePtr();
        const Type *pt2 = pt->getCanonicalTypeUnqualified().getTypePtr();
        switch (pt2->getTypeClass()) {
        case Type::FunctionNoProto :
            return convertFPType((const FunctionNoProtoType *)pt2);
        case Type::FunctionProto :
            return convertFPType((const FunctionProtoType *)pt2);
        default :
            break;
        }
        if (pt->getTypeClass() == Type::Record) {
            const RecordType *rt = (const RecordType *)pt;
            const RecordDecl *decl = rt->getDecl();
            string name = decl->getName().str();
            if (!name.empty() && !recordBodies.count(name))
                return "OpaquePointer";
        }
        string inner = convertType(pt);
        if (inner == "Void")
            return "RawPointer";
        return "Pointer[" + inner + "]";
    }
    case Type::ConstantArray : {
        const ConstantArrayType *t = (const ConstantArrayType *)type;
        ostringstream sout;
        sout << "Array[" << convertType(t->getElementType().getTypePtr());
        sout << "," << t->getSize().getZExtValue() << "]";
        return sout.str();
    }
    case Type::IncompleteArray : {
        const IncompleteArrayType *t = (const IncompleteArrayType *)type;
        const Type *et = t->getElementType().getTypePtr();
        return "Array[" + convertType(et) + ",0]";
    }
    default : {
        string str = type->getCanonicalTypeInternal().getAsString();
        cerr << "warning: unknown type: " << str << '\n';
        return "UnknownType";
    }
    }
}

void BindingsConverter::Initialize(ASTContext &astc) {
    ast = &astc;
}

bool BindingsConverter::declMatches(Decl *decl)
{
    if (matchNames.empty())
        return true;

    clang::SourceLocation sloc = decl->getLocation();
    clang::PresumedLoc ploc = ast->getSourceManager().getPresumedLoc(sloc);
    string filename = ploc.getFilename();

    for (vector<string>::const_iterator i = matchNames.begin();
         i != matchNames.end();
         ++i) {
        if (filename.find(*i) != string::npos)
            return true;
    }
    return false;
}

// Handle top level declarations observed in the program
void BindingsConverter::HandleTopLevelDecl(DeclGroupRef DG)
{
    for (DeclGroupRef::iterator it = DG.begin(); it != DG.end(); ++it) {
        Decl *decl = *it;

        bool matches = declMatches(decl);

        if (matches)
            decls.push_back(decl);

        switch (decl->getKind()) {
        case Decl::Record : {
            RecordDecl *x = (RecordDecl *)decl;
            string name = x->getName().str();
            if (!x->isUnion() &&
                x->isDefinition())
            {
                if (name.empty()) {
                    string outName = allocateName("UnnamedStruct");
                    anonRecordNames[x] = outName;
                }
                else {
                    assert(!recordBodies.count(name));
                    recordBodies[name] = x;
                    string outName = allocateName("Struct_" + name);
                    recordNames[name] = outName;
                }
            }
            break;
        }
        case Decl::Typedef : {
            TypedefDecl *x = (TypedefDecl *)decl;
            const Type *t = x->getTypeSourceInfo()->getType().getTypePtr();
            if (!t->isFunctionType()) {
                string name = x->getName().str();
                string outName = allocateName(name);
                typedefNames[name] = outName;
                cerr << "typedef " << name << " -> " << outName << "\n";
            }
            break;
        }
        case Decl::ObjCInterface : {
            ObjCInterfaceDecl *x = (ObjCInterfaceDecl *)decl;

            if (matches) {
                objcClassNames[x->getNameAsString()] = x;

                for (ObjCContainerDecl::method_iterator imeth = x->meth_begin();
                     imeth != x->meth_end();
                     ++imeth)
                    allocateObjcMethodSelector(*imeth);
                for (ObjCContainerDecl::prop_iterator iprop = x->prop_begin();
                     iprop != x->prop_end();
                     ++iprop)
                    allocateObjcPropertySelector(*iprop);
            }
            break;
        }
        case Decl::ObjCCategory : {
            ObjCCategoryDecl *x = (ObjCCategoryDecl *)decl;

            if (matches) {
                for (ObjCContainerDecl::method_iterator imeth = x->meth_begin();
                     imeth != x->meth_end();
                     ++imeth)
                    allocateObjcMethodSelector(*imeth);
                for (ObjCContainerDecl::prop_iterator iprop = x->prop_begin();
                     iprop != x->prop_end();
                     ++iprop)
                    allocateObjcPropertySelector(*iprop);
            }
        }
        default :
            break;
        }
    }
}

bool BindingsConverter::combineObjcType(QualType *inout_a, QualType b) {
    if (b.getTypePtr()->isVoidType()) {
        return true;
    } else if (inout_a->getTypePtr()->isVoidType()) {
        *inout_a = b;
        return true;
    } else if (inout_a->getTypePtr()->isObjCObjectPointerType()
             && b.getTypePtr()->isObjCObjectPointerType()) {
        *inout_a = ast->getObjCIdType();
        return true;
    } else {
        return false;
    }
}

void BindingsConverter::allocateObjcMethodSelector(ObjCMethodDecl *method)
{
    string selector = method->getSelector().getAsString();

    map<string, selector_info>::iterator existingSelector
        = objcSelectors.find(selector);

    if (existingSelector == objcSelectors.end())
        objcSelectors[selector] = selector_info(method);
    else {
        selector_info &info = existingSelector->second;
        QualType newResultType = method->getResultType();
        if (info.resultType != newResultType) {
            if (!combineObjcType(&info.resultType, newResultType))
                cerr << "warning: conflicting return types for selector \""
                     << selector << "\": "
                     << info.resultType.getAsString() << " vs. "
                     << newResultType.getAsString() << "\n";
        }

        if (!!info.isVariadic != !!method->isVariadic()) {
            cerr << "warning: conflicting variadic-ness for selector \""
                 << selector << "\n";
            info.isVariadic = true;
        }

        unsigned idx = 0;
        ObjCMethodDecl::param_iterator newI = method->param_begin();
        vector<QualType>::iterator infoI = info.paramTypes.begin();
        for (;
             newI != method->param_end() && infoI != info.paramTypes.begin();
             ++newI, ++infoI, ++idx) {

            QualType newType = (*newI)->getType();
            if (*infoI != newType) {
                if (!combineObjcType(&*infoI, newType))
                    cerr << "warning: conflicting types for argument " << idx
                         << " of selector \"" << selector << "\": "
                         << infoI->getAsString() << " vs. "
                         << newType.getAsString() << "\n";
            }
        }
    }
}

void BindingsConverter::allocateObjcPropertySelector(ObjCPropertyDecl *method)
{
    // XXX properties
}

void BindingsConverter::generate()
{
    if (!succeeded)
        return;

    generateHeader();
    generateObjC();
    unsigned i = 0;
    while ((pendingAnonRecords.size() > 0) || (i < decls.size())) {
        if (pendingAnonRecords.size() > 0) {
            vector<RecordDecl*> pending = pendingAnonRecords;
            pendingAnonRecords.clear();
            for (unsigned j = 0; j < pending.size(); ++j)
                generateDecl(pending[j]);
        }
        else if (i < decls.size()) {
            generateDecl(decls[i]);
            ++i;
        }
    }
}

void BindingsConverter::generateObjC()
{
    for (map<string, ObjCInterfaceDecl*>::const_iterator i = objcClassNames.begin();
         i != objcClassNames.end();
         ++i) {
        out << "record " << i->first << " = externalClass(";
        ObjCInterfaceDecl *superclass = i->second->getSuperClass();
        if (superclass)
            out << superclass->getNameAsString();
        else
            out << "Void";
        out << ");\n";
    }

    for (map<string, selector_info>::const_iterator i = objcSelectors.begin();
         i != objcSelectors.end();
         ++i) {
        
        out << "overload "
            << (i->second.isVariadic ? "varargSelector" : "selector")
            << "(static #\"" << i->first << "\") = ";
        out << convertType(i->second.resultType.getTypePtr());

        for (vector<QualType>::const_iterator j = i->second.paramTypes.begin();
             j != i->second.paramTypes.end();
             ++j) {
            out << ", " << convertType(j->getTypePtr());
        }
        out << ";\n";
    }
    out << "\n";
}

bool BindingsConverter::isClayKeyword(const string &cIdent) {
    // list from compiler/src/lexer.cpp:121
    static char const * const keywords[] =
        {"public", "private", "import", "as",
         "record", "variant", "instance",
         "procedure", "overload", "external", "alias",
         "static", "callbyname", "lvalue", "rvalue",
         "inline", "enum", "var", "ref", "forward",
         "and", "or", "not", "new",
         "if", "else", "goto", "return", "while",
         "switch", "case", "default", "break", "continue", "for", "in",
         "true", "false", "try", "catch", "throw", NULL};
    
    for (char const * const *k = keywords; *k; ++k)
        if (cIdent == *k)
            return true;
    return false;
}

string BindingsConverter::convertIdent(const string &cIdent) {
    if (isClayKeyword(cIdent))
        return cIdent + "_";
    else
        return cIdent;
}

void BindingsConverter::generateDecl(Decl *decl)
{
    switch (decl->getKind()) {
    case Decl::Typedef : {
        TypedefDecl *x = (TypedefDecl *)decl;
        string name = x->getName().str();
        const Type *t = x->getTypeSourceInfo()->getType().getTypePtr();
        if (!t->isFunctionType()) {
            string type = convertType(t);
            out << '\n';
            out << "alias " << typedefNames[name] << " = "
                << convertType(t) << ";" << '\n';
        }
        break;
    }
    case Decl::Enum : {
        EnumDecl *x = (EnumDecl *)decl;
        EnumDecl::enumerator_iterator i = x->enumerator_begin();
        EnumDecl::enumerator_iterator e = x->enumerator_end();
        out << '\n';
        for (; i != e; ++i) {
            EnumConstantDecl *y = *i;
            out << "alias " << y->getName().str() << " = "
                << y->getInitVal().getZExtValue() << ";\n";
        }
        break;
    }
    case Decl::Record : {
        RecordDecl *x = (RecordDecl *)decl;
        string name = x->getName().str();
        if (!x->isUnion() &&
            x->isDefinition())
        {
            string outName = name.empty() ?
                anonRecordNames[x] : recordNames[name];
            out << '\n';
            out << "record " << outName << " (\n";
            RecordDecl::field_iterator i, e;
            int index = 0;
            for (i = x->field_begin(), e = x->field_end(); i != e; ++i) {
                FieldDecl *y = *i;
                string fname = y->getName().str();
                const Type *t = y->getType().getTypePtr();
                out << "    ";
                if (fname.empty())
                    out << "unnamed_field" << index;
                else
                    out << convertIdent(fname);
                out << " : " << convertType(t) << ",\n";
                ++index;
            }
            out << ");\n";
        }
        break;
    }
    case Decl::Function : {
        FunctionDecl *x = (FunctionDecl *)decl;
        string name = x->getName().str();
        if (!x->isThisDeclarationADefinition() &&
            !x->isInlineSpecified() &&
            (x->getStorageClass() == SC_Extern ||
             x->getStorageClass() == SC_None) &&
            !externsDeclared.count(name))
        {
            externsDeclared.insert(name);
            out << '\n';
            out << "external ";
            vector<string> attributes;
            if (x->hasAttr<DLLImportAttr>())
                attributes.push_back("dllimport");
            if (x->hasAttr<DLLExportAttr>())
                attributes.push_back("dllexport");
            const Type *t = x->getType().getTypePtr();
            assert(t->isFunctionType());
            const FunctionType *ft = (const FunctionType *)t;
            if (ft->getCallConv() == CC_X86StdCall)
                attributes.push_back("stdcall");
            if (ft->getCallConv() == CC_X86FastCall)
                attributes.push_back("fastcall");
            if (x->hasAttr<AsmLabelAttr>()) {
                const AsmLabelAttr *attr = x->getAttr<AsmLabelAttr>();
                string asmLabel(attr->getLabel());
                attributes.push_back("\"" + asmLabel + "\"");
            }
            if (!attributes.empty()) {
                out << "(";
                for (unsigned i = 0; i < attributes.size(); ++i) {
                    if (i != 0)
                        out << ", ";
                    out << attributes[i];
                }
                out << ") ";
            }
            out << name << "(";
            FunctionDecl::param_iterator i, e;
            int index = 0;
            for (i = x->param_begin(), e = x->param_end(); i != e; ++i) {
                ParmVarDecl *y = *i;
                string pname = y->getName().str();
                if (index != 0)
                    out << ",";
                out << "\n    ";
                if (pname.empty())
                    out << "argument" << index;
                else
                    out << convertIdent(pname);
                const Type *t = y->getType().getTypePtr();
                out << " : " << convertType(t);
                ++index;
            }
            if (ft->getTypeClass() == Type::FunctionProto) {
                const FunctionProtoType *fpt = (const FunctionProtoType *)ft;
                if (fpt->isVariadic()) {
                    if (index != 0)
                        out << ",";
                    out << "\n    ";
                    out << "...";
                }
            }
            out << ")";
            const Type *rt = x->getResultType().getTypePtr();
            string s = convertType(rt);
            if (s != "Void")
                out << " " << s;
            out << ";\n";
        }
        break;
    }
    case Decl::Var : {
        VarDecl *x = (VarDecl *)decl;
        string name = x->getName().str();
        if ((x->getStorageClass() == SC_Extern) &&
            !x->hasInit() &&
            !externsDeclared.count(name))
        {
            externsDeclared.insert(name);
            const Type *t = x->getType().getTypePtr();
            out << '\n';
            out << "external ";
            vector<string> attributes;
            if (x->hasAttr<DLLImportAttr>())
                attributes.push_back("dllimport");
            if (x->hasAttr<DLLExportAttr>())
                attributes.push_back("dllexport");
            if (!attributes.empty()) {
                out << "(";
                for (unsigned i = 0; i < attributes.size(); ++i) {
                    if (i != 0)
                        out << ", ";
                    out << attributes[i];
                }
                out << ") ";
            }
            out << name << " : " << convertType(t) << ";\n";
        }
        break;
    }
    default :
        break;
    }
}

void BindingsConverter::generateHeader()
{
    out << "// Automatically generated by clay-bindgen\n";
    out << "// language: " << language << "\n";
    out << '\n';
    if (language == "objective-c") {
        out << "import cocoa.objc.*;\n";
        out << '\n';
    }
    if (!importNames.empty()) {
        for (vector<string>::const_iterator i = importNames.begin();
             i != importNames.end();
             ++i) {
            out << "import " << *i << ".*;\n";
        }
        out << '\n';
    }
    out << "private alias OpaquePointer = RawPointer;\n";
    out << "private alias UnknownType = Int;\n";
    out << "private alias AUnionType = Int;\n";
    out << '\n';
}

/*
void BindingsConverter::HandleDiagnostic(clang::Diagnostic::Level level, const clang::DiagnosticInfo &info)
{
    if (level == Diagnostic::Error || level == Diagnostic::Fatal)
        succeeded = false;
    this->TextDiagnosticPrinter::HandleDiagnostic(level, info);
}
*/

