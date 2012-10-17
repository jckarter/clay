#ifndef __ERROR_HPP
#define __ERROR_HPP

#include "clay.hpp"

namespace clay {

extern bool shouldPrintFullMatchErrors;
extern set<pair<string,string> > logMatchSymbols;

void matchBindingError(MatchResultPtr const &result);
void matchFailureLog(MatchFailureError const &err);
void matchFailureError(MatchFailureError const &err);

class CompilerError : std::exception {
};

}

#endif // __ERROR_HPP
