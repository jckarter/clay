#include "clay.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include "error.hpp"
#include "loader.hpp"
#include "invoketables.hpp"
#include "env.hpp"

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>

namespace clay {

    typedef llvm::SmallString<16U> Str;

    const char* replAnonymousFunctionName = "__replAnonymousFunction__";

    static ModulePtr module;
    static llvm::ExecutionEngine *engine;

    jmp_buf recovery;

    static bool printAST = false;

    static void eval(llvm::StringRef code, CompilerState* cst);

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

    static vector<Token> addTokens() {
        char buf[255];
        string line = fgets(buf, 255, stdin);
        line = stripSpaces(line);
        SourcePtr source = new Source(line, 0);
        vector<Token> tokens;
        tokenize(source, 0, line.length(), tokens);
        return tokens;
    }
  
    static void cmdGlobals(const vector<Token>& tokens, CompilerState* cst) {
        ModulePtr m;
        if (tokens.size() == 1) {
            m = module;
        } else if (tokens.size() == 2) {
            m = cst->globalModules[tokens[1].str];
        } else {
            llvm::errs() << ":globals [module name]\n";
            return;
        }

        string buf;
        llvm::raw_string_ostream code(buf);

        llvm::StringMap<ObjectPtr>::const_iterator g = m->globals.begin();
        for (; g != m->globals.end(); ++g) {
            if (g->second->objKind == GLOBAL_VARIABLE) {
                IdentifierPtr name = ((GlobalVariable*)g->second.ptr())->name;
                code << "println(\"" << name->str << " : \", " 
                                     << "Type(" << name->str << "), \" : \", "
                                     << name->str <<");\n";
            }
        }
        code.flush();
        eval(buf, cst);
    }

    static void cmdModules(const vector<Token>& tokens, CompilerState* cst) {
        if (tokens.size() != 1) {
            llvm::errs() << "Warning: command parameters are ignored\n";
        }
        llvm::StringMap<ModulePtr>::const_iterator iter = 
            cst->globalModules.begin();
        for (; iter != cst->globalModules.end(); ++iter) {
            llvm::errs() << iter->getKey() << "\n";
        }
    }

    static void cmdOverloads(const vector<Token>& tokens, CompilerState* cst) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i].tokenKind == T_IDENTIFIER) {
                Str identStr = tokens[i].str;

                ObjectPtr obj = lookupPrivate(module, Identifier::get(identStr));
                if (obj == NULL || obj->objKind != PROCEDURE) {
                    llvm::errs() << identStr << " is not a procedure name\n";
                    continue;
                }

                vector<InvokeSet*> sets = lookupInvokeSets(obj.ptr(), cst);
                for (size_t k = 0; k < sets.size(); ++k) {
                    llvm::errs() << "        ";
                    for (size_t l = 0; l < sets[k]->argsKey.size(); ++l) {
                        llvm::errs() << sets[k]->argsKey[l] << " : ";
                    }
                    llvm::errs() << "\n";
                }
            }
        }
    }

    static void cmdPrint(const vector<Token>& tokens) {
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
    }

    static void replCommand(string const& line, CompilerState* cst)
    {
        SourcePtr source = new Source(line, 0);
        vector<Token> tokens;
        //TODO: don't use compiler's tokenizer
        tokenize(source, 0, line.length(), tokens);
        Str cmd = tokens[0].str;
        if (cmd == "q") {
            exit(0);
        } else if (cmd == "globals") {
            cmdGlobals(tokens, cst);
        } else if (cmd == "modules") {
            cmdModules(tokens, cst);
        } else if (cmd == "overloads") {
            cmdOverloads(tokens, cst);
        } else if (cmd == "print") {
            cmdPrint(tokens);
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

    static void loadImports(llvm::ArrayRef<ImportPtr> imports, CompilerState* cst)
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

    static void jitTopLevel(llvm::ArrayRef<TopLevelItemPtr> toplevels)
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

    static void jitStatements(llvm::ArrayRef<StatementPtr> statements, 
                              CompilerState* cst)
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
                new ExternalProcedure(module.ptr(),
                                      fun,
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
        codegenAfterRepl(ctor, dtor, cst);

        engine->runFunction(ctor, std::vector<llvm::GenericValue>());

        void* dtorLlvmFun = engine->getPointerToFunction(dtor);
        typedef void (*PFN)();
        atexit((PFN)(uintptr_t)dtorLlvmFun);
        engine->runFunction(entryProc->llvmFunc, std::vector<llvm::GenericValue>());
    }

    static void jitAndPrintExpr(ExprPtr expr, CompilerState* cst) {
        //expr -> println(expr);
        NameRefPtr println = new NameRef(Identifier::get("println"));
        ExprPtr call = new Call(println.ptr(), new ExprList(expr));
        ExprStatementPtr callStmt = new ExprStatement(call);
        jitStatements(vector<StatementPtr>(1, callStmt.ptr()), cst);
    }

    static void eval(llvm::StringRef line, CompilerState* cst) {
        SourcePtr source = new Source(line, 0);
        try {
            ReplItem x = parseInteractive(source, 0, source->size(), cst);
            if (x.isExprSet) {
                jitAndPrintExpr(x.expr, cst);
            } else {
                loadImports(x.imports, cst);
                jitTopLevel(x.toplevels);
                jitStatements(x.stmts, cst);
            }
        }
        catch (CompilerError) {
            return;
        }        
    }

    static void interactiveLoop(CompilerState* cst)
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
                replCommand(line.substr(1, line.size() - 1), cst);
            } else {
                eval(line, cst);
            }
        }
        engine->runStaticConstructorsDestructors(true);
    }

    static void exceptionHandler(int i)
    {
        llvm::errs() << "SIGABRT called\n";
        longjmp(recovery, 1);
    }



    void runInteractive(ModulePtr module_)
    {
        signal(SIGABRT, exceptionHandler);
        module = module_;
        llvm::errs() << "Clay interpreter\n";
        llvm::errs() << ":q to exit\n";
        llvm::errs() << ":print {identifier} to print an identifier\n";
        llvm::errs() << ":modules to list global modules\n";
        llvm::errs() << ":globals to list globals\n";
        llvm::errs() << "In multi-line mode empty line to exit\n";

        CompilerState* cst = module_->cst;
        llvm::EngineBuilder eb(cst->llvmModule);
        llvm::TargetOptions targetOptions;
        targetOptions.JITExceptionHandling = true;
        eb.setTargetOptions(targetOptions);
        engine = eb.create();
        engine->runStaticConstructorsDestructors(false);

        interactiveLoop(cst);
    }

}
