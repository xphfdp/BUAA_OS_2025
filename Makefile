.PHONY: clean

check: check.c
	gcc -c check.c -o check.o

all: 
	gcc src/main.c src/output.c -o main
	mv src/main 23371506/out

run: 
	./out/main

clean:
	rm -rf check.o out/main
