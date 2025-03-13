#!/bin/bash
a=$1
b=$2
if [ $a -eq 0 -a $b -eq 0 ]
then
	cat stderr.txt
elif [ $a -gt 0 -a $b -eq 0]
then
	awk 'NR>=${a} {print $0}' stderr.txt
else
	awk 'NR>=${a}&&NR<${b} {print $0}' stderr.txt
fi
