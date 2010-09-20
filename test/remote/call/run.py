from subprocess import Popen, PIPE
from sys import argv

server = Popen([argv[1], "server"], stdin=PIPE, stdout=PIPE, stderr=PIPE)
client = Popen([argv[1], "client", "zim", "zang", "zung"],
    stdin=server.stdout, stdout=server.stdin, stderr=PIPE)

servererr = server.communicate()[1]
clienterr = client.communicate()[1]

print "-- server stderr"
print servererr,
print "-- client stderr"
print clienterr,
