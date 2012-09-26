#include "clay.hpp"
#include <iostream>
#include <string>
#include <sstream>

namespace clay {

using std::ostringstream;

const char* replAnonymousFunctionName = "__replAnonymousFunction__";

void runInteractive(llvm::Module *llvmModule, ModulePtr module)
{
    setExitOnError(false);
    llvm::EngineBuilder eb(llvmModule);
    llvm::ExecutionEngine *engine = eb.create();
    engine->runStaticConstructorsDestructors(false);
    string line;
    int lineNum = 0;
    while(true) {
        std::cout << "clay>";
        std::cin >> line;
        SourcePtr source = new Source(line, 0);
        vector<Token> tokens;
        tokenize(source, 0, source->size(), tokens);

        vector<StatementPtr> statements;
        try {
            parseInteractive(source, 0, source->size(), statements);
        }
        catch (std::exception) {
            continue;
        }

        ostringstream funName;
        funName << replAnonymousFunctionName << lineNum;
        IdentifierPtr fun = Identifier::get(funName.str());
        ++lineNum ;

        BlockPtr funBody = new Block(statements);
        ExternalProcedurePtr entryProc =
                new ExternalProcedure(fun,
                                    PRIVATE,
                                    vector<ExternalArgPtr>(),
                                    false,
                                    NULL,
                                    funBody.ptr(),
                                    new ExprList());

        entryProc->env = module->env;
        try {
            codegenExternalProcedure(entryProc, true);
        }
        catch (std::exception) {
            continue;
        }

        void* fPtr = engine->getPointerToFunction(entryProc->llvmFunc);
        void (*f)() = (void (*)())fPtr;
        f();
    }
    engine->runStaticConstructorsDestructors(true);
}

}
