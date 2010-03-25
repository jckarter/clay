/* vim: set sw=4 ts=4: */
/*
 * Shell.cpp
 * Implements a readline shell
 */

#include <string>
#include <set>
#include <cstring>
#include <iostream>
#include <sstream>
#include <exception>
using namespace std;
#include <readline/readline.h>
#include <readline/history.h>

#include "Shell.h"

static void trim(string& str);

Shell *Shell::instance = NULL;
// Initially defined keywords
Shell::Shell() {
    const char* cmds[]
    {
        "help",
        "quit"
    };
    int cmdsLen = sizeof(cmds)/sizeof(cmds[0]);

    // Populate shell
    for(int i=0; i<cmdsLen; i++) {
        string cmd = strdup(cmds[i]);
        keywords.insert(cmd);
    }

    init_readline();
}

Shell::~Shell() {

}

Shell* Shell::getInstance() {
    if(instance == NULL) {
        instance = new Shell();
    }
    return instance;
}

void Shell::run(ShellConsumer& consumer) {
    Shell* instance = getInstance();

    instance->isRunning = true;
    stringstream promptstr(stringstream::in|stringstream::out);
    promptstr << "clay$ ";

    while(instance->isRunning)
    {
        string str;
        char* c_str;

        if ((c_str = readline(promptstr.str().c_str())) == NULL) break;
        str = c_str;
        trim(str);

        if (str == "")  continue;

        add_history(str.c_str());

        if(str == "quit")
        {
            instance->isRunning = false;
        }
        else if(str == "help")
        {
            //printHelp();
        }
        else
        {
            try
            {
                consumer.consume(str);
            }
            catch(exception &e)
            {
                cout << "Exception Caught: " << e.what() << endl;
            }
        }
    }
    cout << endl;
}

void Shell::init_readline() {
    rl_attempted_completion_function = (rl_completion_func_t *)&Shell::cmd_completion;
}

char** Shell::cmd_completion(const char* text, int start, int end) {
    char** matches;
    matches = (char**)NULL;

    matches = rl_completion_matches(text, (rl_compentry_func_t *)&Shell::cmd_generator);

    return matches;
}

char* Shell::cmd_generator(const char* text, int state) {
    Shell* instance = getInstance();

    static StringSet::const_iterator it;
    int len;

    if(state == 0)
    {
        it = instance->keywords.begin();
        len =  strlen(text);
    }

    while(it != instance->keywords.end())
    {
        const char* str = (*it++).c_str();
        if(strncmp(str, text, len) == 0)
            return (strdup(str));
    }

    return ((char*)NULL);
}

static void trim(string& str) {
    string::iterator it;
    for(it = str.begin(); *it==' ' || *it=='\t' || *it=='\n'; it++);
    if( it != str.begin()) str.erase(str.begin(), it);

    for(it = str.end()-1; *it==' ' || *it=='\t' || *it=='\n'; it--);
    if( it != str.end()) str.erase(it+1, str.end());
}

