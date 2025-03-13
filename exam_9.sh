#!/bin/bash
c=0
d=0
a=$1
b=$2
((c=a))
((d=b))
if [ $c -eq 0 -a $d -eq 0 ]
then
	cat stderr.txt
elif [ $c -gt 0 -a $d -eq 0 ]
then
	awk 'NR>=${c} {print $0}' stderr.txt
else
	awk 'NR>=${c}&&NR<${d} {print $0}' stderr.txt
fi
