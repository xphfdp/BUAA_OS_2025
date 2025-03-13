#!/bin/bash
mkdir codeSet
cp code/wa.sy codeSet/wa.c
sed -i '1i\#include"include/libsy.h"' codeSet/wa.c
sed -i 's/getInt/getint/g' codeSet/wa.c
i=0
for((i=1;i<=10;i++)); do
	cp code/code${i}.sy codeSet/code${i}.c
	sed -i '1i\#include"include/libsy.sh"' codeSet/code${i}.c
	sed -i 's/getInt/getint/g' codeSet/code${i}.c
done
