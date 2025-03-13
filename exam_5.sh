#!/bin/bash
a=0
for((a=0;a<=20;a++)); do
	sed -i "s/REPLACE/${a}/g" origin/code/${a}.c
	mv origin/code/${a}.c 23371506/result/code
done

