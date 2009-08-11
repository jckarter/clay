machineops.dylib : machineops/machineops.cpp
	g++ -dynamiclib -Wall -o $@ $^

clean :
	rm -f machineops.dylib
