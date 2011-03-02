LOCAL_INCLUDE = /opt/local/include
CLANG_INCLUDE = /Users/ks/projects/llvm-release/lib/clang/2.9/include

generated.clay: 
	clay-bindgen -I$(LOCAL_INCLUDE) -I$(CLANG_INCLUDE) -match lua -match lauxlib -import unix -import libc -o generated.clay generated_from.h
