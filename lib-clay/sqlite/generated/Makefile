generated.clay: 
	clay-bindgen -match sqlite3 -import unix -import libc -o generated.clay generated_from.h

windows32:
	gcc -E generated_from.h > temp32.h
	clay-bindgen -match sqlite3 -import win32 -import libc -o generated.windows.32.clay temp32.h
	del /f temp32.h
