/* vim: set sw=4 ts=4: */
/*
 * clay-dbg.cpp
 * This program is the entry point for the clay debugger
 */

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace std;

#include "Shell.h"
#include "ShellConsumer.h"

class ClayConsumer : public ShellConsumer {
    public:
        ClayConsumer() { }
        virtual ~ClayConsumer() { }

        virtual void consume(string str);
};

void ClayConsumer::consume(string str) {
    cout << str << endl;
}

int main(int argc, char *argv[])
{
    ClayConsumer clay;

    Shell::run(clay);

    return 0;
}


