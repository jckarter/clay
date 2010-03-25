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

class EchoConsumer : public ShellConsumer {
    public:
        EchoConsumer() { }
        virtual ~EchoConsumer() { }

        virtual void consume(string str);
};

void EchoConsumer::consume(string str) {
    cout << str << endl;
}

int main(int argc, char *argv[])
{
    EchoConsumer echo;

    Shell::run(echo);

    return 0;
}


