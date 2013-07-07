#pragma once


#include <vector>

#include "clay.hpp"

namespace clay {

CodePtr clone(CodePtr x);
void clone(llvm::ArrayRef<PatternVar> x, vector<PatternVar> &out);
void clone(llvm::ArrayRef<IdentifierPtr> x, vector<IdentifierPtr> &out);
ExprPtr clone(ExprPtr x);
ExprPtr cloneOpt(ExprPtr x);
ExprListPtr clone(ExprListPtr x);
void clone(llvm::ArrayRef<FormalArgPtr> x, vector<FormalArgPtr> &out);
vector<FormalArgPtr> clone(llvm::ArrayRef<FormalArgPtr> x);
FormalArgPtr clone(FormalArgPtr x);
FormalArgPtr cloneOpt(FormalArgPtr x);
void clone(llvm::ArrayRef<ReturnSpecPtr> x, vector<ReturnSpecPtr> &out);
ReturnSpecPtr clone(ReturnSpecPtr x);
ReturnSpecPtr cloneOpt(ReturnSpecPtr x);
StatementPtr clone(StatementPtr x);
StatementPtr cloneOpt(StatementPtr x);
void clone(llvm::ArrayRef<StatementPtr> x, vector<StatementPtr> &out);
CaseBlockPtr clone(CaseBlockPtr x);
void clone(llvm::ArrayRef<CaseBlockPtr> x, vector<CaseBlockPtr> &out);
CatchPtr clone(CatchPtr x);
void clone(llvm::ArrayRef<CatchPtr> x, vector<CatchPtr> &out);

} // namespace clay
