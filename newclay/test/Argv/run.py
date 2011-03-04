from subprocess import check_call, CalledProcessError
from sys import argv

check_call([argv[1], "hello", "world"])
