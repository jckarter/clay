example: foo.s example.s
	gcc -o example foo.s example.s

foo.s: foo.clay
	clay -asm foo.clay
example.s: example.clay
	clay -asm example.clay

.PHONY: clean

clean:
	rm -f example *.o *.s *.ll
