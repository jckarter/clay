#include "clay.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <setjmp.h>
#include <signal.h>

#include <llvm/Target/TargetOptions.h>


namespace clay {

    using std::ostringstream;

    const char* replAnonymousFunctionName = "__replAnonymousFunction__";

    static ModulePtr module;
    static llvm::ExecutionEngine *engine;

    jmp_buf recovery;
    static void interactiveLoop()
    {
        setjmp(recovery);
        string line;
        static int lineNum = 0;
        while(true) {
            std::cout.flush();
            std::cout << "clay>";
            getline (std::cin, line);
            if (line == ":q")
                break;

            SourcePtr source = new Source(line, 0);
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

            engine->runFunction(entryProc->llvmFunc, std::vector<llvm::GenericValue>());
        }
        engine->runStaticConstructorsDestructors(true);
    }

    static void exceptionHandler(int i)
    {
        longjmp(recovery, 1);
    }

    void runInteractive(llvm::Module *llvmModule_, ModulePtr module_)
    {
        signal(SIGABRT, exceptionHandler);

        llvmModule = llvmModule_;
        module = module_;

        std::cout << "Clay interpreter\n";
        std::cout << ":q to exit\n";

        setExitOnError(false);
        llvm::EngineBuilder eb(llvmModule);
        engine = eb.create();
        engine->runStaticConstructorsDestructors(false);

        interactiveLoop();
    }

}
