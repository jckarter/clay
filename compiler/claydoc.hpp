#pragma once


#include <string>
#include <vector>
#include <iostream>
#include <map>

#include "clay.hpp"


struct DocObject
{
    std::string name;
    std::string description;
    clay::TopLevelItemPtr item;
};

struct DocSection
{
    std::string name;
    std::string description;
    std::vector<DocObject *> objects;

};

struct DocModule
{
    std::string fqn;
    std::string name;
    std::string fileName;
    std::vector<DocSection*> sections;
    std::string description;
};

struct DocState
{
    std::string name;
    std::map<std::string, DocModule *> references;
    std::map<std::string, DocModule *> modules;
};


std::string identifierString(clay::IdentifierPtr id);

void emitHtmlModule (std::string outpath, DocModule *mod, std::string fqn);
void emitHtmlIndex  (std::string outpath, DocState  *);
