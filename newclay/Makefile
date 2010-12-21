all:
	@echo "Choose a test* target"
	@exit 1

lexer.s: newclay/common/common.clay newclay/diagnostics/diagnostics.clay newclay/lexer/lexer.clay newclay/lexer/external/external.clay
	clay -asm -shared -o lexer.s newclay/lexer/external/external.clay

parser.s: lexer.o newclay/ast/ast.clay newclay/parser/parser.clay newclay/parser/external/external.clay
	clay -asm -shared -o parser.s newclay/parser/external/external.clay

%.s: %.clay
	clay -asm -o $@ $<

%: %.o lexer.o parser.o
	gcc -o $@ $^

clean:
	rm -f *.o *.s
