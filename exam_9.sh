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
	i=0
	for((i=c;i<=22;i++)); do
		sed -n "${i}p" stderr.txt
	done
else
	i=0
	for((i=c;i<=d;i++)); do
		sed -n "${i}p" stderr.txt
	done
fi
