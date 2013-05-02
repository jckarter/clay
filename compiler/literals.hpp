#pragma once


#include "clay.hpp"

namespace clay {

ValueHolderPtr parseIntLiteral(ModulePtr module, IntLiteral *x);
ValueHolderPtr parseFloatLiteral(ModulePtr module, FloatLiteral *x);

}
