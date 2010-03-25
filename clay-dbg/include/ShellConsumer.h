/* vim: set sw=4 ts=4: */
/*
 * ShellConsumer.h
 * Receives input from the shell
 */

#ifndef SHELL_CONSUMER_H
#define SHELL_CONSUMER_H

#include <string>
using namespace std;

class ShellConsumer {

public:
    ShellConsumer();
    virtual ~ShellConsumer();

    virtual void consume(string str)=0;
};

#endif // SHELL_CONSUMER_H
