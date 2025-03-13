#!/bin/bash
a=0
for((a=0;a<=20;a++)); do
	sed -i "s/REPLACE/${a}/g" origin/code/${a}.c
	mv origin/code/${a}.c result/code
done

