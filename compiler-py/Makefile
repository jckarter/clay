machineops.dylib : machineops/machineops.cpp
	g++ -dynamiclib -Wall -o $@ $^

.PHONY: clean
clean :
	rm -f machineops.dylib
