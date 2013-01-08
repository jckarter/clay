#ifndef __ENV_HPP
#define __ENV_HPP


#include "clay.hpp"


namespace clay {

void addGlobal(ModulePtr module,
               IdentifierPtr name,
               Visibility visibility,
               ObjectPtr value);
ObjectPtr lookupPrivate(ModulePtr module, IdentifierPtr name);
ObjectPtr lookupPublic(ModulePtr module, IdentifierPtr name);
ObjectPtr safeLookupPublic(ModulePtr module, IdentifierPtr name);

void addLocal(EnvPtr env, IdentifierPtr name, ObjectPtr value);
ObjectPtr lookupEnv(EnvPtr env, IdentifierPtr name);
ObjectPtr safeLookupEnv(EnvPtr env, IdentifierPtr name);
ModulePtr safeLookupModule(EnvPtr env);
llvm::DINameSpace lookupModuleDebugInfo(EnvPtr env);

ObjectPtr lookupEnvEx(EnvPtr env, IdentifierPtr name,
                      EnvPtr nonLocalEnv, bool &isNonLocal,
                      bool &isGlobal);

ExprPtr foreignExpr(EnvPtr env, ExprPtr expr);

ExprPtr lookupCallByNameExprHead(EnvPtr env);
Location safeLookupCallByNameLocation(EnvPtr env);

} // namespace clay

#endif // __ENV_HPP
