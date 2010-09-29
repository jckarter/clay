from subprocess import Popen;
from sys import argv, stdout
from time import sleep

server = Popen([argv[1], "server"]);
sleep(1)


print "-- client 1: address"
stdout.flush()
client = Popen([argv[1], "client", "address"]);
client.wait()

print "-- client 2: garbage"
stdout.flush()
client = Popen([argv[1], "client", "garbage"]);
client.wait()

print "-- client 3: quit"
stdout.flush()
client = Popen([argv[1], "client", "quit"]);
client.wait()

server.wait()
sleep(1)
