#include "claydoc.hpp"
#include "parser.hpp"
#include <sstream>
#include <string>

using namespace std;
using namespace clay;

static CompilerState* cst = new CompilerState();

static void usage(char *argv0)
{
    llvm::errs() << "usage: " << argv0 << " <sourceDir> <htmlOutputDir>\n";
}

DocModule *docParseModule(string fileName, DocState *state, std::string fqn)
{
    SourcePtr src = new Source(fileName);
    ModulePtr m = parse(fileName, src, cst, ParserKeepDocumentation);

    DocModule *docMod = new DocModule;
    docMod->fileName = fileName;
    docMod->fqn = fqn;

    DocSection *section = new DocSection;
    docMod->sections.push_back(section);

    DocumentationPtr lastAttachment;

    for (vector<TopLevelItemPtr>::iterator i = m->topLevelItems.begin(); i != m->topLevelItems.end(); i++) {
        TopLevelItemPtr item = *i;
        if (!item)
            continue;
        std::string name = identifierString(item->name);

        switch (item->objKind) {
            case DOCUMENTATION: {
                DocumentationPtr doc = ((Documentation*)item.ptr());
                if (doc->annotation.count(ModuleAnnotation)) {
                    docMod->name = doc->annotation[ModuleAnnotation];
                    docMod->description = doc->text;

                } else if (doc->annotation.count(SectionAnnotation)) {
                    section = new DocSection;
                    section->name = doc->annotation[SectionAnnotation];
                    section->description = doc->text;
                    docMod->sections.push_back(section);

                } else {
                    lastAttachment = doc;
                }
                break;
            }
            case OVERLOAD:
                if (!!((clay::Overload *)item.ptr())->target)
                    name = ((clay::Overload *)item.ptr())->target->asString();
            case RECORD_DECL:
            case PROCEDURE: {
                DocObject *obj = new DocObject;
                obj->item = item;
                obj->name = name;
                if (!!lastAttachment) {
                    obj->description = lastAttachment->text;
                    lastAttachment = 0;
                }
                section->objects.push_back(obj);

                if (item->objKind != OVERLOAD)
                    state->references.insert(std::pair<std::string, DocModule*>(name, docMod));

                break;
            }
            default: {} // make compiler happy
        }
    }
    state->modules.insert(std::pair<std::string, DocModule*>(fqn, docMod));

    return docMod;
}

bool endsWith (std::string const &fullString, std::string const &ending)
{
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

int main(int argc, char **argv)
{
    string inputDir;
    string outputDir;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-help") == 0
                || strcmp(argv[i], "--help") == 0
                || strcmp(argv[i], "/?") == 0) {
            usage(argv[0]);
            return 2;
        }
        else if (strstr(argv[i], "-") != argv[i]) {
            if (!inputDir.empty()) {
                if (!outputDir.empty()) {
                    usage(argv[0]);
                    return 1;
                }
                outputDir = argv[i];
                continue;
            }
            inputDir = argv[i];
        }
    }

    if (inputDir.empty() || outputDir.empty()) {
        usage(argv[0]);
        return 1;
    }

    bool whatever;
    if (llvm::sys::fs::create_directories(outputDir, whatever) != 0) {
         llvm::errs() << "cannot create output directory " << outputDir << "\n";
         return 4;
    }

    DocState *state = new DocState;
    state->name = llvm::sys::path::filename(inputDir).str();

    llvm::error_code ec;
    for (llvm::sys::fs::recursive_directory_iterator it(inputDir, ec), ite; it != ite; it.increment(ec)) {
        llvm::sys::fs::file_status status;
        if (!it->status(status) && is_regular_file(status) && endsWith(it->path(), ".clay")) {

            std::string fqn;

            llvm::sys::path::const_iterator word = llvm::sys::path::begin(it->path());
            ++word;

            llvm::sys::path::const_iterator worde = llvm::sys::path::end(it->path());
            --worde;

            for (; word != worde; ++word) {
                fqn.append(*word);
                fqn.append(".");
            }

            llvm::StringRef stemR = llvm::sys::path::stem(it->path());
            fqn.append(string(stemR.begin(), stemR.end()));

            std::string fileName = it->path();
            llvm::errs() << "parsing " << fileName << "\n";

            docParseModule(fileName, state, fqn);
        }
    }

    emitHtmlIndex(outputDir, state);
}


std::string identifierString(clay::IdentifierPtr id)
{
    if (!id)
        return std::string("<anonymous>");
    return string(id->str.str().begin(), id->str.str().end());
}
