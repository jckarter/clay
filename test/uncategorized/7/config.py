def match(output) :
    refoutput = open("out.txt").read()
    return output.replace('\r', '') == refoutput.replace('\r', '')
