pipe=/tmp/remote-call-pipe.$$
servererr=/tmp/remote-call-server-stderr.$$
clienterr=/tmp/remote-call-client-stderr.$$
mkfifo $pipe || exit 1

($1 server < $pipe 2>$servererr) | ($1 client zim zang zung > $pipe 2>$clienterr)

echo -- server stderr
cat $servererr

echo -- client stderr
cat $clienterr

rm $pipe
rm $servererr
rm $clienterr

