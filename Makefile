.PHONY: clean
all:
	gcc -I src/include src/main.c src/output.c -o main
	mv main out

check: check.c
	gcc -c check.c -o check.o

run: 
	./out/main

clean:
	rm -rf check.o out/main
