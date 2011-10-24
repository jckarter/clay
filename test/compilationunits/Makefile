example: foo.s example.s
	gcc -o example foo.s example.s

foo.s: foo.clay
	clay -shared -asm foo.clay
example.s: example.clay
	clay -asm example.clay
