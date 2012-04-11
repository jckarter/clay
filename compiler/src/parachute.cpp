#include "clay.hpp"

#if defined(_WIN32) || defined(_WIN64)

# ifdef _MSC_VER
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

namespace clay {

// SEH parachute

static int emergencyCompileContext(LPEXCEPTION_POINTERS exceptionInfo)
{
    fprintf(stderr, "exception code 0x%08X!\n", exceptionInfo->ExceptionRecord->ExceptionCode);
    __try {
        displayCompileContext();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        fprintf(stderr, "exception code 0x%08X displaying compile context!\n", GetExceptionCode());
    }
    fflush(stderr);
    llvm::errs().flush();
    return EXCEPTION_CONTINUE_SEARCH;
}

int parachute(int (*mainfn)(int, char **, char const* const*),
    int argc, char **argv, char const* const* envp)
{
    __try {
        return mainfn(argc, argv, envp);
    }
    __except (emergencyCompileContext(GetExceptionInformation())) {}
    assert(false);
    return 86;
}

}

# else

// FIXME should at least try to use VEH for mingw/cygwin

namespace clay {

int parachute(int (*mainfn)(int, char **, char const* const*),
    int argc, char **argv, char const* const* envp)
{
    return mainfn(argc, argv, envp);
}

}

# endif

#else

// Signal parachute

#include <signal.h>

namespace clay {

static volatile int threatLevel = 0;

static void emergencyCompileContext(int sig) {
    int oldThreatLevel = __sync_fetch_and_add(&threatLevel, 1);
    if (oldThreatLevel == 0) {
        fprintf(stderr, "signal %d!\n", sig);
        displayCompileContext();
    } else {
        fprintf(stderr, "signal %d displaying compile context!\n", sig);
    }
    fflush(stderr);
    llvm::errs().flush();
    signal(sig, SIG_DFL);
    raise(sig);
}

int parachute(int (*mainfn)(int, char **, char const* const*),
    int argc, char **argv, char const* const* envp)
{
    signal(SIGSEGV, emergencyCompileContext);
    signal(SIGBUS, emergencyCompileContext);
    signal(SIGILL, emergencyCompileContext);
    signal(SIGABRT, emergencyCompileContext);
    return mainfn(argc, argv, envp);
}

}

#endif
