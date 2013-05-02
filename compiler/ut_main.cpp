#include <vector>

#include "ut.hpp"
#include "parachute.hpp"

using namespace std;

namespace clay {

    struct Test {
        const char* name;
        TestFunc func;
    };

    static vector<Test>* tests;

    void register_test(const char* name, TestFunc func) {
        if (tests == 0) {
            tests = new vector<Test>;
        }
        tests->push_back(Test());
        tests->back().name = name;
        tests->back().func = func;
    }

    int real_main(int argc, char **argv, char const* const* envp) {
        for (vector<Test>::const_iterator test = tests->begin(); test != tests->end(); ++test) {
            printf("%s...\n", test->name);
            try {
                test->func();

                printf("%s OK\n", test->name);
            } catch (const AssertionError&) {
                printf("%s FAILED\n", test->name);
            }
        }
        return 0;
    }
}

int main(int argc, char **argv, char const* const* envp) {
    return clay::parachute(&clay::real_main, argc, argv, envp);
}
