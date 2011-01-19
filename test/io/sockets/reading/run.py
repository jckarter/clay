from subprocess import Popen, PIPE;
from sys import argv
from time import sleep

server = Popen([argv[1], "server"], stdout=PIPE);

# wait for handshake from server before starting client
hello = server.stdout.read(5);

client = Popen([argv[1], "client"]);
client.wait()

server.wait()
