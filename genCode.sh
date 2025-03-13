#!/bin/bash
mkdir codeSet
cp code/wa.sy codeSet/wa.c
sed -i '1i\#include"include/libsy.h"' codeSet/wa.c
sed -i 's/getInt/getint/g' codeSet/wa.c
a=0
for((a=1;a<=10;a++)); do
	cp code/code${a}.sy codeSet/code${a}.c
	sed -i '1i\#include"include/libsy.sh"' codeSet/code${a}.c
	sed -i 's/getInt/getint/g' codeSet/code${a}.c
done
