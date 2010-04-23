/* vim: set sw=4 ts=4: */
/*
 * Shell.h
 * Implements a readline shell
 */

#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <set>
#include "ShellConsumer.h"

using namespace std;

typedef set<string> StringSet;
class Shell {

public:
    virtual ~Shell();

    static void run(ShellConsumer& consumer);
    static char** cmd_completion(const char* text, int start, int end);
    static char* cmd_generator(const char* text, int state);

private:
    static Shell* instance;

    static Shell* getInstance();

    Shell();
    void init_readline();
    bool isRunning;

    StringSet keywords;
};

#endif // SHELL_H
