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

    string newFunctionName()
    {
        static int funNum = 0;
        ostringstream funName;
        funName << replAnonymousFunctionName << funNum;
        ++funNum;
        return funName.str();
    }

    string stripSpaces(string const& s)
    {
        size_t i;
        for (i = 0; i < s.size(); ++i) {
            if (!isSpace(s[i]))
                break;
        }
        return s.substr(i, s.length());
    }

    static void replCommand(string const& line)
    {
        typedef llvm::SmallString<16U> Str;
        SourcePtr source = new Source(line, 0);
        vector<Token> tokens;
        tokenize(source, 0, line.length(), tokens);
        Str cmd = tokens[0].str;
        if (cmd == "q") {
            exit(0);
        } else if (cmd == "print") {
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i].tokenKind == T_IDENTIFIER) {
                    Str identifier = tokens[i].str;
                    llvm::StringMap<ImportSet>::const_iterator iter = module->allSymbols.find(identifier);
                    if (iter == module->allSymbols.end()) {
                        std::cout << "Can't find identifier " << identifier.c_str();
                    } else {
                        for (size_t i = 0; i < iter->second.size(); ++i) {
                            llvm::errs() << iter->second[i] << "\n";
                        }
                    }
                }
            }
        }
        else {
            std::cout << "Unknown interactive mode command: " << cmd.c_str() << "\n";
        }
    }

    static void jitTopLevel(string const& line)
    {
        SourcePtr source = new Source(line, 0);
        vector<TopLevelItemPtr> toplevels;
        parseTopLevelItems(source, 0, line.length(), toplevels);
        for (size_t i = 0; i < toplevels.size(); ++i) {
                    llvm::errs() << i << ": " << toplevels[i] << "\n";
        }

        //this doesn't work
        addGlobals(module, toplevels);
        //module->initialized = false;
        //initModule(module);
    }

    static void jitStatements(string const& line)
    {
        SourcePtr source = new Source(line, 0);
        vector<StatementPtr> statements;

        try {
            parseInteractive(source, 0, source->size(), statements);
        }
        catch (std::exception) {
            return;
        }

        for (size_t i = 0; i < statements.size(); ++i) {
            llvm::errs() << statements[i] << "\n";
        }

        IdentifierPtr fun = Identifier::get(newFunctionName());

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
            return;
        }

        engine->runFunction(entryProc->llvmFunc, std::vector<llvm::GenericValue>());
    }

    static void interactiveLoop()
    {
        setjmp(recovery);
        string line;
        while(true) {
            std::cout.flush();
            std::cout << "clay>";
            getline (std::cin, line);
            line = stripSpaces(line);
            if (line[0] == ':') {
                replCommand(line.substr(1, line.size() - 1));
            }else if (line[0] == '^')	{
                jitTopLevel(line.substr(1, line.size() - 1));
            }
            else {
                jitStatements(line);
            }
        }
        engine->runStaticConstructorsDestructors(true);
    }

    static void exceptionHandler(int i)
    {
        llvm::errs() << "SIGABRT called\n";
        longjmp(recovery, 1);
    }



    void runInteractive(llvm::Module *llvmModule_, ModulePtr module_)
    {
        signal(SIGABRT, exceptionHandler);

        llvmModule = llvmModule_;
        module = module_;

        llvm::errs() << "Clay interpreter\n";
        llvm::errs() << ":q to exit\n";
        llvm::errs() << ":print {identifier} to print an identifier\n";

        setExitOnError(false);
        llvm::EngineBuilder eb(llvmModule);
        llvm::TargetOptions targetOptions;
        targetOptions.JITExceptionHandling = true;
        eb.setTargetOptions(targetOptions);
        engine = eb.create();
        engine->runStaticConstructorsDestructors(false);

        interactiveLoop();
    }

}
