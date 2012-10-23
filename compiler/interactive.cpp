#include "clay.hpp"
#include "lexer.hpp"
#include "codegen.hpp"
#include "error.hpp"
#include "loader.hpp"

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>

namespace clay {

    using std::ostringstream;

    const char* replAnonymousFunctionName = "__replAnonymousFunction__";

    static ModulePtr module;
    static llvm::ExecutionEngine *engine;

    jmp_buf recovery;

    static bool printAST = false;

    string newFunctionName()
    {
        static int funNum = 0;
        string buf;
        llvm::raw_string_ostream funName(buf);
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
                        llvm::errs() << "Can't find identifier " << identifier.c_str();
                    } else {
                        for (size_t i = 0; i < iter->second.size(); ++i) {
                            llvm::errs() << iter->second[i] << "\n";
                        }
                    }
                }
            }
        } else if (cmd == "ast_on") {
            printAST = true;
        } else if (cmd == "ast_off") {
            printAST = false;
        } else if (cmd == "rebuild") {
            //TODO : this command should re-codegen everything
        }
        else {
            llvm::errs() << "Unknown interactive mode command: " << cmd.c_str() << "\n";
        }
    }

    static void loadImports(llvm::ArrayRef<ImportPtr>  imports)
    {
        for (size_t i = 0; i < imports.size(); ++i) {
            module->imports.push_back(imports[i]);
        }
        for (size_t i = 0; i < imports.size(); ++i) {
            loadDependent(module, NULL, imports[i], false);
        }
        for (size_t i = 0; i < imports.size(); ++i) {
            initModule(imports[i]->module);
        }
    }

    static void jitTopLevel(llvm::ArrayRef<TopLevelItemPtr>  toplevels)
    {
        if (toplevels.empty()) {
            return;
        }
        if (printAST) {
            for (size_t i = 0; i < toplevels.size(); ++i) {
                llvm::errs() << i << ": " << toplevels[i] << "\n";
            }
        }
        addGlobals(module, toplevels);
    }

    static void jitStatements(llvm::ArrayRef<StatementPtr>  statements)
    {
        if (statements.empty()) {
            return;
        }

        if (printAST) {
            for (size_t i = 0; i < statements.size(); ++i) {
                llvm::errs() << statements[i] << "\n";
            }
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

        codegenBeforeRepl(module);
        try {
            codegenExternalProcedure(entryProc, true);
        }
        catch (std::exception) {
            return;
        }

        llvm::Function* ctor;
        llvm::Function* dtor;
        codegenAfterRepl(ctor, dtor);

        engine->runFunction(ctor, std::vector<llvm::GenericValue>());

        void* dtorLlvmFun = engine->getPointerToFunction(dtor);
        typedef void (*PFN)();
        atexit((PFN)(uintptr_t)dtorLlvmFun);
        engine->runFunction(entryProc->llvmFunc, std::vector<llvm::GenericValue>());
    }

    static void interactiveLoop()
    {
        setjmp(recovery);
        string line;
        while(true) {
            llvm::errs().flush();
            llvm::errs() << "clay>";
            char buf[255];
            line = fgets(buf, 255, stdin);
            line = stripSpaces(line);
            if (line[0] == ':') {
                replCommand(line.substr(1, line.size() - 1));
            } else {
                SourcePtr source = new Source(line, 0);
                vector<StatementPtr> stmts;
                vector<TopLevelItemPtr> toplevels;
                vector<ImportPtr> imports;
                try {
                    parseInteractive(source, 0, source->size(), toplevels, imports, stmts);
                    loadImports(imports);
                    jitTopLevel(toplevels);
                    jitStatements(stmts);
                }
                catch (CompilerError) {
                    continue;
                }
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

        llvm::EngineBuilder eb(llvmModule);
        llvm::TargetOptions targetOptions;
        targetOptions.JITExceptionHandling = true;
        eb.setTargetOptions(targetOptions);
        engine = eb.create();
        engine->runStaticConstructorsDestructors(false);

        interactiveLoop();
    }

}
