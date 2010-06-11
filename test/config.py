def cmdline(clay):
    return [clay, "main.clay"]

def pre_build():
    pass

def post_build():
    pass

def post_run():
    pass

def match(output):
    refoutput = open("out.txt").read()
    return output == refoutput
