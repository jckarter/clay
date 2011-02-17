all: clay

.PRECIOUS: %.o %.s

lexer.s: newclay/common/common.clay newclay/diagnostics/diagnostics.clay newclay/lexer/lexer.clay newclay/lexer/external/external.clay
	clay $(CLAYFLAGS) -asm -shared -o lexer.s newclay/lexer/external/external.clay

parser.s: lexer.o newclay/ast/ast.clay newclay/parser/parser.clay newclay/parser/external/external.clay
	clay $(CLAYFLAGS) -asm -shared -o parser.s newclay/parser/external/external.clay

%.o: %.clay .forced
	clay $(CLAYFLAGS) -c -o $@ $<

.forced:

#%.s: %.clay .forced
#	clay $(CLAYFLAGS) -asm -o $@ $<

%.o: %.s
	gcc -c -o $@ $<

%: %.o lexer.o parser.o
	gcc -o $@ $^

clay: main.o lexer.o parser.o
	gcc -o $@ $^

clean:
	rm -f *.o *.s

.PHONY: clean .forced
