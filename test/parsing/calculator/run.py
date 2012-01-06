from subprocess import check_call, CalledProcessError
from sys import argv

try:
    check_call([argv[1], '1+2*3'])
    check_call([argv[1], '2*3+1'])
except CalledProcessError as ex:
    print "!! error code", ex.returncode
