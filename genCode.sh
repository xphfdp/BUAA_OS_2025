#!/bin/bash
mkdir codeSet
sed -i '1i\#include"include/libsy.h"' code/wa.sy
sed -i 's/getInt/getint/g' code/wa.sy
cp code/wa.sy codeSet/wa.c
i=0
for((i=1;i<=10;i++)); do
	sed -i '1i\#include"include/libsy.sh"' code/code${i}.sy
	sed -i 's/getInt/getint/g' code/code${i}.sy
	cp code/${i}.sy codeSet/code${i}.c
done
