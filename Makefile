.PHONY: clean

out: calc case_all
	gcc -o calc calc.c
	cat case_all | ./calc 1> out	
# Your code here.
case_add: casegen.c
	gcc -o casegen casegen.c
	./casegen add 100 1> case_add
case_sub: casegen.c
	gcc -o casegen casegen.c
	./casegen sub 100 1> case_sub
case_mul: casegen.c
	gcc -o casegen casegen.c
	./casegen mul 100 1> case_mul
case_div: casegen.c
	gcc -o casegen casegen.c
	./casegen div 100 1> case_div
case_all: casegen.c
	gcc -o casegen casegen.c
	./casegen add 100 1> case_add
	./casegen sub 100 1> case_sub
	./casegen mul 100 1> case_mul
	./casegen div 100 1> case_div
	cat case_add case_sub case_mul case_div > case_all
clean:
	rm -f out calc casegen case_* *.o
