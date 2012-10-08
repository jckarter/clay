from subprocess import check_call, CalledProcessError
from sys import argv

try:
    check_call([argv[1], 'apples', 'bananas', 'orange soda'])
except CalledProcessError as ex:
    print "!! error code", ex.returncode
