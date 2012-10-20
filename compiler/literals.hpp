#ifndef __LITERALS_HPP
#define __LITERALS_HPP

#include "clay.hpp"

namespace clay {

ValueHolderPtr parseIntLiteral(ModulePtr module, IntLiteral *x);
ValueHolderPtr parseFloatLiteral(ModulePtr module, FloatLiteral *x);

}

#endif // __LITERALS_HPP
