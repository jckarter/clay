from subprocess import check_call, CalledProcessError
from sys import argv

try:
    check_call([argv[1]])
except CalledProcessError as ex:
    print "!! error code", ex.returncode
check_call([argv[1], "add", "1", "2"])
check_call([argv[1], "subtract", "4", "3"])
check_call([argv[1], "multiply", "5", "6"])
check_call([argv[1], "divide", "8", "4"])
try:
    check_call([argv[1], "divide", "1", "0"])
except CalledProcessError as ex: 
    print "!! error code", ex.returncode
check_call([argv[1], "version"])
