import os, sys

def genC(f, includes, constants) :
    print>>f, "#include <stdio.h>"
    for include in includes :
        print>>f, "#include <%s>" % include
    print>>f, ""
    print>>f, "int main() {"
    for constant in constants :
        print>>f, "    printf(\"static %s = %%d;\\n\", %s);" % (constant, constant)
    print>>f, "    return 0;"
    print>>f, "}"

def main() :
    if len(sys.argv) != 2 :
        print "usage: constgen.py <constfile>"
        print "<constfile> should contain a list of " + \
            "'include <header>' lines and '<constant>' lines."
        return
    includes = []
    constants = []
    for line in file(sys.argv[1]) :
        line = line.strip()
        if line.startswith("include ") :
            include = line[len("include "):]
            includes.append(include)
        elif len(line) > 0 :
            constants.append(line)
    cname = "__constgen_temp.c"
    ename = "__constgen_temp.out"
    f = file(cname, "w")
    genC(f, includes, constants)
    f.close()
    os.system("gcc -o %s %s" % (ename, cname))
    os.system(os.path.join(".", ename))
    os.remove(ename)
    os.remove(cname)

if __name__ == "__main__" :
    main()
